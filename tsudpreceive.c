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

#define UDP_MAXIMUM_SIZE \
  57528 /* maior multiplo entre 188 e 204 mais proximo de 65535 */

long long int msecDiff(struct timespec *time_stop,
                       struct timespec *time_start) {
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
  return ((result.tv_sec * 1000000000) + result.tv_nsec) / 1000000;
}

int main(int argc, char *argv[]) {
  struct timespec time_start;
  struct timespec time_stop;

  memset(&time_start, 0, sizeof(time_start));
  memset(&time_stop, 0, sizeof(time_stop));

  int sockfd;
  struct sockaddr_in addr;
#ifdef ip_mreqn
  struct ip_mreqn mgroup;
  XXX
#else
  /* according to
       http://lists.freebsd.org/pipermail/freebsd-current/2007-December/081080.html
     in bsd it is also possible to simply use ip_mreq instead of ip_mreqn
     (same as in Linux), so we are using this instead
  */
  struct ip_mreq mgroup;
#endif
      int reuse;
  unsigned int addrlen;
  int len;
  unsigned char udp_packet[UDP_MAXIMUM_SIZE];

  if (argc != 4) {
    fprintf(stderr, "Usage: %s ip_addr port bytes_sent_ bitrate> output.ts\n",
            argv[0]);
    return 0;
  } else {
    memset((char *)&mgroup, 0, sizeof(mgroup));
    mgroup.imr_multiaddr.s_addr = inet_addr(argv[1]);
#ifdef ip_mreqn
    mgroup.imr_address.s_addr = INADDR_ANY;
#else
    /* this is called 'interface' here */
    mgroup.imr_interface.s_addr = INADDR_ANY;
#endif
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addrlen = sizeof(addr);
  }

  int bytes_sent_bitrate = atoi(argv[3]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket(): error ");
    return 0;
  }

  reuse = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
                 sizeof(reuse)) < 0) {
    perror("setsockopt() SO_REUSEADDR: error ");
  }

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind(): error");
    close(sockfd);
    return 0;
  }

  setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mgroup,
             sizeof(mgroup));

  clock_gettime(CLOCK_MONOTONIC, &time_start);
  int bytes_sent = 0;
  int max_errors_rcv = 0;
  int max_errors_wrt = 0;
  while (1) {
    if (max_errors_rcv >= 5) {
      fprintf(stderr, "Max errors in sequence ocurred\n");
      exit(EXIT_FAILURE);
    }

    if (max_errors_wrt >= 5) {
      fprintf(stderr, "Max errors in sequence ocurred\n");
      exit(EXIT_FAILURE);
    }

    if (bytes_sent_bitrate && bytes_sent >= bytes_sent_bitrate) {
      clock_gettime(CLOCK_MONOTONIC, &time_stop);
      long long msec = msecDiff(&time_stop, &time_start);
      clock_gettime(CLOCK_MONOTONIC, &time_start);

      long long bytes_ll = (bytes_sent);
      long long rx_rate_bytes_ms = bytes_ll / msec;
      long long rx_rate_bits_s = rx_rate_bytes_ms * 8000;
      fprintf(stderr, "Rx rate: %lld bits/s\n", rx_rate_bits_s);
      bytes_sent = 0;
    }

    len = recvfrom(sockfd, udp_packet, UDP_MAXIMUM_SIZE, 0,
                   (struct sockaddr *)&addr, &addrlen);
    if (len < 0) {
      max_errors_rcv++;
      perror("recvfrom(): error ");
      continue;
    }
    max_errors_rcv = 0;

    if (bytes_sent_bitrate) {
      bytes_sent += len;
    }

    int n = write(STDOUT_FILENO, udp_packet, len);
    if (n < 0) {
      max_errors_wrt++;
      fprintf(stderr, "Error on write in file");
      continue;
    }
    max_errors_wrt = 0;
  }
}
