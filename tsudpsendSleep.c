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
#define _GNU_SOURCE
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

long long int msecDiff(struct timespec* time_stop,
                       struct timespec* time_start) {
  struct timespec result;
  long long int temp = 0;
  long long int utemp = 0;

  if (!time_stop || !time_start) {
    fprintf(stderr, "memory is garbaged?\n");
    return -1;
  }

  result.tv_sec = time_stop->tv_sec - time_start->tv_sec;
  result.tv_nsec = time_stop->tv_nsec - time_start->tv_nsec;
  if (result.tv_nsec < 0) {
    --result.tv_sec;
    result.tv_nsec += 1000000000L;
  }

  if (result.tv_sec < 0) {
    fprintf(stderr, "start time %ld.%ld is after stop time %ld.%ld\n",
            time_start->tv_sec, time_start->tv_nsec, time_stop->tv_sec,
            time_stop->tv_nsec);
    return -1;
  }
  return ((result.tv_sec * 1000000000) + result.tv_nsec);
}

int main(int argc, char* argv[]) {
  struct timespec time_start;
  struct timespec time_stop;

  memset(&time_start, 0, sizeof(time_start));
  memset(&time_stop, 0, sizeof(time_stop));

  int sockfd;
  int len;
  int total_lido;
  int size;
  int sent;
  int sent_bytes = 0;
  int ret;
  int is_multicast;
  int transport_fd;
  int bytes_log_refresh;
  unsigned char option_ttl;
  char start_addr[4];
  struct sockaddr_in addr;
  unsigned long int packet_size;
  char* tsfile;
  unsigned char* send_buf;
  unsigned int sleep = 0;

  memset(&addr, 0, sizeof(addr));
  if (argc < 5) {
    fprintf(stderr,
            "Usage: %s file.ts ipaddr port sleep [ts_packet_per_ip_packet] "
            "[bytes_log_refresh] [udp_packet_ttl]\n",
            argv[0]);
    fprintf(stderr, "ts_packet_per_ip_packet default is 7\n");
    fprintf(stderr, "sleep u seconds \n");
    return 0;
  } else {
    tsfile = argv[1];
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[2]);
    addr.sin_port = htons(atoi(argv[3]));

    sleep = atoi(argv[4]);

    if (argc >= 6) {
      packet_size = strtoul(argv[5], 0, 0) * TS_PACKET_SIZE;
    } else {
      packet_size = 7 * TS_PACKET_SIZE;
    }
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket(): error ");
    return 0;
  }

  size = 57528; /* maior multiplo entre 188 e 204 mais proximo de 65535 */
  ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));

  if (argc >= 7) {
    bytes_log_refresh = atoi(argv[6]);
  }

  if (argc >= 8) {
    option_ttl = atoi(argv[7]);
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

    
  if (strncmp(tsfile,"/dev/stdin",10) == 0){
    transport_fd = stdin;
    fprintf(stderr, "Set pipesize\n");
    fcntl(transport_fd, F_SETPIPE_SZ, 4*size);
  }else{
    transport_fd = open(tsfile, O_RDONLY);
      if (transport_fd < 0) {
      fprintf(stderr, "can't open file %s\n", tsfile);
      close(sockfd);
      return 0;
    }
  }

  int pipe_size =  fcntl(transport_fd, F_GETPIPE_SZ);
  fprintf(stderr, "Pipe size: %d\n", pipe_size);




  int completed = 0;
  send_buf = malloc(packet_size);

  // ler=packet_size;

  while (!completed) {
    total_lido = 0;

    /*		while ((total_lido < packet_size) && (len >= 0))
                    {

    //			ler=packet_size - total_lido;

    //			len = read(transport_fd, &send_buf[total_lido] , ler);
                            len = read(transport_fd, &send_buf[total_lido] ,
    packet_size - total_lido);




                            if(len <= 0)
                            return 0;

                            if(len > 0)
                            {
                                    //if (send_buf[0] == 0x47){
                                            total_lido=total_lido+len;
                                    //	ler=packet_size - total_lido;
                                    //}else{
                                    //	ler=1;
                                    //	total_lido=0;
                                    //	fprintf(stderr, "RESYNC \n");
                                    //}
                            }
                    }
    */
    total_lido = 0;
    while ((total_lido < packet_size) && (len >= 0)) {
      // fprintf(stderr,"1");

      len = read(transport_fd, &send_buf[total_lido], packet_size - total_lido);

      if (len <= 0) return 0;

      if (len > 0) total_lido = total_lido + len;
      // else
      //       nanosleep(&nano_sleep_packet2, 0);
    }

    if (len < 0) {
      fprintf(stderr, "ts file read error \n");
      completed = 1;
    } else if (len == 0) {
      fprintf(stderr, "ts sent done\n");
      completed = 1;
    } else {
      sent = sendto(sockfd, send_buf, packet_size, 0, (struct sockaddr*)&addr,
                    sizeof(struct sockaddr_in));
      if (sent <= 0) {
        perror("send(): error ");
        completed = 1;
      }

      if (bytes_log_refresh) {
        sent_bytes = sent_bytes + sent;
      }
    }


    if (bytes_log_refresh && sent_bytes >= bytes_log_refresh) {
      clock_gettime(CLOCK_MONOTONIC, &time_stop);
      long long msec = msecDiff(&time_stop, &time_start);
      memcpy(&time_start, &time_stop, sizeof(time_start));

      long long bytes_ll = (sent_bytes);
      long long int tx_rate_bits_s = (bytes_ll * 8000000000) / msec;
      //      long long rx_rate_bits_s = rx_rate_bytes_ms * 8000;
      fprintf(stderr, "Rate: %lld bps\n", tx_rate_bits_s);
      sent_bytes = 0;
    }

    if (sleep > 0) usleep(sleep);
  }

  close(transport_fd);
  close(sockfd);
  free(send_buf);
  return 0;
}
