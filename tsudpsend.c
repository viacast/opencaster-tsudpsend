/*
 * Copyright (C) 2008-2013, Lorenzo Pallara l.pallara@avalpa.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#define MULTICAST

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TS_PACKET_SIZE 188
#define MAX_PID 8192
#define SYSTEM_CLOCK_FREQUENCY 27000000

long long int usecDiff(struct timespec* time_stop,
                       struct timespec* time_start) {
  long long int temp = 0;
  long long int utemp = 0;

  if (time_stop && time_start) {
    if (time_stop->tv_nsec >= time_start->tv_nsec) {
      utemp = time_stop->tv_nsec - time_start->tv_nsec;
      temp = time_stop->tv_sec - time_start->tv_sec;
    } else {
      utemp = time_stop->tv_nsec + 1000000000 - time_start->tv_nsec;
      temp = time_stop->tv_sec - 1 - time_start->tv_sec;
    }
    if (temp >= 0 && utemp >= 0) {
      temp = (temp * 1000000000) + utemp;
    } else {
      fprintf(stderr, "start time %ld.%ld is after stop time %ld.%ld\n",
              time_start->tv_sec, time_start->tv_nsec, time_stop->tv_sec,
              time_stop->tv_nsec);
      temp = -1;
    }
  } else {
    fprintf(stderr, "memory is garbaged?\n");
    temp = -1;
  }
  return temp / 1000;
}

int get_bitrate(int* bitrate_fd, int packet_size_udp, double* bitrate,
                int* n_packets_udp, int* file_over) {
  int len = 0;
  int has_bitrate = 0;
  u_short pid;
  unsigned int pcr_ext = 0;
  unsigned long long int pcr_base = 0;
  unsigned long long int new_pcr = 0;
  unsigned long long int new_pcr_index = 0;
  unsigned char* bitrate_buf;
  unsigned long long int ref_packets = 0;
  static unsigned long long int ts_packet_count = 0;
  static unsigned long long int
      pid_pcr_table[MAX_PID]; /* PCR table for the TS packets */
  static unsigned long long int
      pid_pcr_index_table[MAX_PID]; /* PCR index table for the TS packets */

  bitrate_buf = malloc(packet_size_udp);
  /* Read next packet */

  fprintf(stderr, "error\n");

  while (1) {
    /* Get packet UDP */
    // TODO tratar os ultimos pacotes.
    int total_lido = 0;
    if (ts_packet_count % (packet_size_udp / TS_PACKET_SIZE) == 0) {
      if (has_bitrate) {
        // fprintf(stderr, "has bitrate\n");
        free(bitrate_buf);

        lseek(*bitrate_fd, -((*n_packets_udp) * packet_size_udp), SEEK_CUR);

        break;
      }

      ref_packets = ts_packet_count;
      while (total_lido < packet_size_udp) {
        len = read(*bitrate_fd, bitrate_buf + total_lido,
                   packet_size_udp - total_lido);

        if (len == 0) {
          if (total_lido > 0) {
            *file_over = 1;
            fprintf(stderr,
                    "File Over \t N_last packets: %d \t N_last bytes: %d\n\n",
                    *n_packets_udp, total_lido);

            lseek(*bitrate_fd, -total_lido, SEEK_CUR);
            lseek(*bitrate_fd, -((*n_packets_udp) * packet_size_udp), SEEK_CUR);

            return 0;
          }
          // fprintf(stdout, "Acabou o arquivo \n\n");
          // fprintf(stdout, "N packets ts : %llu \n\n", ts_packet_count);
          exit(EXIT_SUCCESS);
        }

        if (len < 0) {
          fprintf(stderr, "Error\n");
          exit(EXIT_FAILURE);
        }

        if (len > 0) total_lido = total_lido + len;
      }
      *n_packets_udp = *n_packets_udp + 1;
    }

    unsigned char* ts_packet =
        (bitrate_buf + ((ts_packet_count - ref_packets) * TS_PACKET_SIZE));

    /* check packet */
    memcpy(&pid, ts_packet + 1, 2); /* get the 2nd and 3 bytes*/

    pid = ntohs(pid);
    pid = pid & 0x1fff; /* get pid of ts */
    if (pid < MAX_PID) {
      int has_adaptation_field = ts_packet[3] & 0x20;
      int adaptation_field_length = ts_packet[4];
      int has_pcr_field = ts_packet[5] & 0x10;
      if (has_adaptation_field && (adaptation_field_length != 0) &&
          has_pcr_field) {
        uint64_t pcr_base_first_byte = ts_packet[6] << 25;
        uint64_t pcr_base_second_byte = ts_packet[7] << 17;
        uint64_t pcr_base_third_byte = ts_packet[8] << 9;
        uint64_t pcr_base_fourth_byte = ts_packet[9] << 1;
        uint64_t pcr_base_last_bit_33 = ts_packet[10] >> 7;

        uint16_t pcr_ext_first_bit = (ts_packet[10] & 1) << 8;
        uint16_t pcr_ext_last_byte = ts_packet[11];

        pcr_base = (pcr_base_first_byte) + (pcr_base_second_byte) +
                   (pcr_base_third_byte) + (pcr_base_fourth_byte) +
                   (pcr_base_last_bit_33);

        pcr_ext = pcr_ext_first_bit + pcr_ext_last_byte;

        new_pcr = pcr_base * 300 + pcr_ext;
        new_pcr_index = (ts_packet_count * TS_PACKET_SIZE) + 10;

        has_bitrate = 1;
        *bitrate = (((double)(new_pcr_index - pid_pcr_index_table[pid])) * 8 *
                    SYSTEM_CLOCK_FREQUENCY) /
                   ((double)(new_pcr - pid_pcr_table[pid]));

        fprintf(stderr, "Pid : %u\n", pid);
        fprintf(stderr, "new pcr_index : %llu\n", new_pcr_index);
        fprintf(stderr, "old pcr_index : %llu\n", pid_pcr_index_table[pid]);
        fprintf(stderr, "new pcr : %llu\n", new_pcr);
        fprintf(stderr, "old pcr : %llu\n", pid_pcr_table[pid]);
        fprintf(stderr, "Meu bitrate: %f\n", *bitrate);

        fprintf(stderr, "Indice delta is : %llu bytes \n\n\n",
                new_pcr_index - pid_pcr_index_table[pid]);

        pid_pcr_table[pid] = new_pcr;
        pid_pcr_index_table[pid] = new_pcr_index;
      }
    }
    ts_packet_count++;
  }

  return 0;
}

int get_sleep(struct timespec* nanosleep, double bitrate,
              unsigned long int packet_size) {
  int one_second_nano = 1000000000;
  int size_udp_packet_bits = packet_size * 8;
  double n_packets = (bitrate / size_udp_packet_bits);

  nanosleep->tv_nsec = (one_second_nano / n_packets);
  fprintf(stderr, "Nano sleep: %lu\n\n", nanosleep->tv_nsec);
  return 0;
}

int main(int argc, char* argv[]) {
  int sockfd;
  int n_packets_udp = 0;
  int len;
  int sent;
  int file_over = 0;
  int ret;
  int is_multicast;
  int transport_fd;
  unsigned char option_ttl;
  char start_addr[4];
  struct sockaddr_in addr;
  unsigned long int packet_size;
  char* tsfile;
  unsigned char* send_buf;
  double bitrate;
  unsigned long long int packet_time;
  unsigned long long int real_time;
  struct timespec time_start;
  struct timespec time_stop;
  struct timespec nano_sleep_packet;

  memset(&addr, 0, sizeof(addr));
  memset(&time_start, 0, sizeof(time_start));
  memset(&time_stop, 0, sizeof(time_stop));
  memset(&nano_sleep_packet, 0, sizeof(nano_sleep_packet));

  if (argc < 4) {
    fprintf(stderr,
            "Usage: %s file.ts ipaddr port  [ts_packet_per_ip_packet] "
            "[udp_packet_ttl]\n",
            argv[0]);
    fprintf(stderr, "ts_packet_per_ip_packet default is 7\n");
    fprintf(stderr, "bit rate refers to transport stream bit rate\n");
    fprintf(stderr, "zero bitrate is 100.000.000 bps\n");
    return 0;
  }

  tsfile = argv[1];
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(argv[2]);
  addr.sin_port = htons(atoi(argv[3]));
  // bitrate = atoi(argv[4]);
  // if (bitrate <= 0) {
  //   bitrate = 100000000;
  // }
  if (argc >= 5) {
    packet_size = strtoul(argv[4], 0, 0) * TS_PACKET_SIZE;
  } else {
    packet_size = 7 * TS_PACKET_SIZE;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket(): error ");
    return 0;
  }

  if (argc >= 6) {
    option_ttl = atoi(argv[5]);
    is_multicast = 0;
    memcpy(start_addr, argv[2], 3);
    start_addr[3] = 0;
    is_multicast = atoi(start_addr);
    is_multicast = (is_multicast >= 224) || (is_multicast <= 239);
    if (is_multicast) {
      ret = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &option_ttl,
                       sizeof(option_ttl));
    } else {
      ret = setsockopt(sockfd, IPPROTO_IP, IP_TTL, &option_ttl,
                       sizeof(option_ttl));
    }

    if (ret < 0) {
      perror("ttl configuration fail");
    }
  }

  transport_fd = open(tsfile, O_RDONLY);
  if (transport_fd < 0) {
    fprintf(stderr, "can't open file %s\n", tsfile);
    close(sockfd);
    return 0;
  }

  int completed = 0;
  send_buf = malloc(packet_size);
  packet_time = 0;
  real_time = 0;
  // nano_sleep_packet.tv_nsec = 665778; /* 1 packet at 100mbps*/

  while (!completed) {
    n_packets_udp = 0;
    get_bitrate(&transport_fd, packet_size, &bitrate, &n_packets_udp,
                &file_over);
    get_sleep(&nano_sleep_packet, bitrate, packet_size);

    // fprintf(stderr, "N_packets : %d \t bitrate: %f \t\n", n_packets_udp,
    //         bitrate);

    clock_gettime(CLOCK_MONOTONIC, &time_start);

    packet_time = 0;
    int i = 0;
    for (i = 0; i < n_packets_udp; i++) {
      // fprintf(
      //     stderr,
      //     "Bitrate: %f \t real_time: %llu Teorical bits: %.0f \t Sent bits: "
      //     "%llu \t Completed: %d\n",
      //     bitrate, real_time, real_time * bitrate / (double)1000000,
      //     packet_time, completed);

      // len = read(transport_fd, send_buf, packet_size);
      int total_lido = 0;
      while (total_lido < packet_size) {
        len =
            read(transport_fd, send_buf + total_lido, packet_size - total_lido);

        if (len <= 0) {
          fprintf(stdout,
                  "Acabou o arquivo \t Valor de i: %d \t N_packets: %d \t Len: "
                  "%d\n\n",
                  i, n_packets_udp, len);
          exit(EXIT_SUCCESS);
        }

        if (len > 0) total_lido = total_lido + len;
      }

      sent = sendto(sockfd, send_buf, len, 0, (struct sockaddr*)&addr,
                    sizeof(struct sockaddr_in));

      if (sent <= 0) {
        perror("send(): error ");
        completed = 1;
        break;
      }

      packet_time += packet_size * 8;
      clock_gettime(CLOCK_MONOTONIC, &time_stop);
      real_time = usecDiff(&time_stop, &time_start);

      // packet_time * 1000000 is to adjsust with real_time
      if (real_time * bitrate < packet_time * 1000000) {
        nanosleep(&nano_sleep_packet, 0);
      }
    }

    if (file_over) {
      len = read(transport_fd, send_buf, packet_size);
      sent = sendto(sockfd, send_buf, len, 0, (struct sockaddr*)&addr,
                    sizeof(struct sockaddr_in));

      if (sent <= 0) {
        perror("send(): error ");
        completed = 1;
        break;
      }

      fprintf(stderr, "Last bytes");
      break;
    }
  }
  close(transport_fd);
  close(sockfd);
  free(send_buf);
  return 0;
}
