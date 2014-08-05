/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Sergey Arnautov, Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#ifndef _TMASCO_SC_H_
#define _TMASCO_SC_H_

/* Typedefs for wrapped system calls */

#include <sys/socket.h>

typedef int (socket_f)(int domain, int type, int protocol);
typedef int (close_f)(int fd);
typedef int (bind_f)(int sockfd, const struct sockaddr *addr,
                     socklen_t addrlen);
typedef int (connect_f)(int socket, const struct sockaddr *addr,
                        socklen_t length);
typedef ssize_t (send_f)(int socket, const void *buffer, size_t size,
                         int flags);

#endif /* _TMASCO_SC_H_ */
