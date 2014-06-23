/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H 1

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdbool.h>
#include "openvswitch/types.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>

int set_nonblocking(int fd);
void xset_nonblocking(int fd);
int set_dscp(int fd, uint8_t dscp);

int lookup_ip(const char *host_name, struct in_addr *address);
int lookup_ipv6(const char *host_name, struct in6_addr *address);

int lookup_hostname(const char *host_name, struct in_addr *);

int get_socket_rcvbuf(int sock);
int check_connection_completion(int fd);
void drain_fd(int fd, size_t n_packets);
ovs_be32 guess_netmask(ovs_be32 ip);

bool inet_parse_active(const char *target, uint16_t default_port,
                       struct sockaddr_storage *ssp);
int inet_open_active(int style, const char *target, uint16_t default_port,
                     struct sockaddr_storage *ssp, int *fdp, uint8_t dscp);

bool inet_parse_passive(const char *target, int default_port,
                        struct sockaddr_storage *ssp);
int inet_open_passive(int style, const char *target, int default_port,
                      struct sockaddr_storage *ssp, uint8_t dscp,
                      bool kernel_print_port);

int read_fully(int fd, void *, size_t, size_t *bytes_read);
int write_fully(int fd, const void *, size_t, size_t *bytes_written);

int fsync_parent_dir(const char *file_name);
int get_mtime(const char *file_name, struct timespec *mtime);

char *describe_fd(int fd);

/* Default value of dscp bits for connection between controller and manager.
 * Value of IPTOS_PREC_INTERNETCONTROL = 0xc0 which is defined
 * in <netinet/ip.h> is used. */
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)

/* Functions for working with sockaddr_storage that might contain an IPv4 or
 * IPv6 address. */
uint16_t ss_get_port(const struct sockaddr_storage *);
#define SS_NTOP_BUFSIZE (1 + INET6_ADDRSTRLEN + 1)
char *ss_format_address(const struct sockaddr_storage *,
                        char *buf, size_t bufsize);
size_t ss_length(const struct sockaddr_storage *);
const char *sock_strerror(int error);

#ifndef _WIN32
void xpipe(int fds[2]);
void xpipe_nonblocking(int fds[2]);

int drain_rcvbuf(int fd);

int make_unix_socket(int style, bool nonblock,
                     const char *bind_path, const char *connect_path);
int get_unix_name_len(socklen_t sun_len);

/* Helpers for calling ioctl() on an AF_INET socket. */
struct ifreq;
int af_inet_ioctl(unsigned long int command, const void *arg);
int af_inet_ifreq_ioctl(const char *name, struct ifreq *,
                        unsigned long int cmd, const char *cmd_name);

#define closesocket close
#endif

#ifdef _WIN32
/* Windows defines the 'optval' argument as char * instead of void *. */
#define setsockopt(sock, level, optname, optval, optlen) \
    rpl_setsockopt(sock, level, optname, optval, optlen)
static inline int rpl_setsockopt(int sock, int level, int optname,
                                 const void *optval, socklen_t optlen)
{
    return (setsockopt)(sock, level, optname, optval, optlen);
}

#define getsockopt(sock, level, optname, optval, optlen) \
    rpl_getsockopt(sock, level, optname, optval, optlen)
static inline int rpl_getsockopt(int sock, int level, int optname,
                                 void *optval, socklen_t *optlen)
{
    return (getsockopt)(sock, level, optname, optval, optlen);
}
#endif

/* In Windows platform, errno is not set for socket calls.
 * The last error has to be gotten from WSAGetLastError(). */
static inline int sock_errno(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

#endif /* socket-util.h */
