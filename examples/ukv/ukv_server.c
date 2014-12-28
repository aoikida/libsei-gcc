/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "ukv.h"
#include "ukv_net.h"
#include <tmasco.h>

#define BUFSIZE 1024

int
main(const int argc, const char* argv[])
{
    int fd;
    struct sockaddr_in addr;
    int port;

    if (argc < 2) {
        printf("usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    port = atoi(argv[1]);

    // create socket, bind and listen
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // this is important, so that if a process restarts, it can
    // quickly reuse the same port.
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(fd, 2) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }

    // initialize ukv service
    ukv_t* ukv = ukv_init();
    __asco_prepare_nm();
    __asco_begin(1);
    ukv = ukv_init();
    __asco_end(1);

    while (1) {

        // accept client connections
        char buffer[BUFSIZE+1];
        ssize_t read;
        int cfd;
        struct sockaddr_in caddr;
        socklen_t len;

        len = sizeof(caddr);
        cfd = accept(fd, (struct sockaddr*) &caddr, &len);
        if (cfd < 0) {
            perror("accept");
            continue;
        }

        // once a connection is accepted, read from the connection
        // until it is closed
        while (1) {
            const char* r = NULL;
            read = recvfrom(cfd, buffer, BUFSIZE, 0,
                            (struct sockaddr*) &caddr, &len);
            if (read <= 0) {
                perror("recvfrom");
                break;
            }

            buffer[read] = '\0';
            printf("received: %s", buffer);

            // we assume that we received the whole message. This is
            // definitely not correct.

            __asco_prepare_nm();
            __asco_begin(2);
            r =  ukv_recv(ukv, buffer);
            __asco_end(2);
            if (!r) goto fini;

            sendto(cfd, r, strlen(r), 0,
                   (struct sockaddr*) &caddr, sizeof(caddr));
            printf("replied: %s\n", r);

            __asco_prepare_nm();
            __asco_begin(3);
            ukv_done(ukv, r);
            __asco_end(3);

            printf("---------------------\n");
        }
        close(cfd);
    }
  fini:
    close(fd);

    __asco_prepare_nm();
    __asco_begin(4);
    ukv_fini(ukv);
    __asco_end(4);

    return 0;
}
