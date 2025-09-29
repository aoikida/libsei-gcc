/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens, Sergei Arnautov 
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

/* Temporarily mask system functions to avoid conflicts with libsei */
#include <sei.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "ukv.h"
#include "ukv_net.h"

#define BUFSIZE 1024

enum {INIT, ACCT, RECV, PROC, SEND, FINI, CORR} state;

int
main(const int argc, const char* argv[])
{
    if (argc < 2) {
        printf("usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd, cfd, port;
    struct sockaddr_in addr, caddr;
    ssize_t read;
    socklen_t len;
    char buffer[BUFSIZE+1];
    char*  msg;
    const char* r;
    ukv_t* ukv;
#ifndef SEI_DISABLED
    ssize_t msg_len;
    uint32_t crc; 
#endif

    len   = sizeof(caddr);
    port  = atoi(argv[1]);
    state = INIT;

    while(1) {
        switch(state) {
            case INIT: {
                // create socket, bind and listen
                fd = socket(AF_INET, SOCK_STREAM, 0);
                if (fd < 0) {
                    perror("socket");
                    return EXIT_FAILURE;
                }

                // this is important, so that if a process restarts, it can
                // quickly reuse the same port
                int on = 1;
                if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, 
                               sizeof(on)) < 0) {
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
                __begin_nm();
                ukv = ukv_init();
                __end();

                state = ACCT;
                break;
            }
            case ACCT: 
                // accept client connections
                cfd = accept(fd, (struct sockaddr*) &caddr, &len);
                if (cfd < 0) {
                    perror("accept");
                    break;
                }

                state = RECV;
                break;
            case RECV: {
                // once a connection is accepted, read from the connection
                // until it is closed
                read = recvfrom(cfd, buffer, BUFSIZE, 0,
                                (struct sockaddr*) &caddr, &len);
                if (read <= 0) {
                    perror("recvfrom");
                    close(cfd);

                    state = ACCT;
                    break;
                }

                buffer[read] = '\0';
                
                // we assume that we received the whole message. This is
                // definitely not correct.
                
#ifndef SEI_DISABLED
                // assume first four bytes are CRC of the request
                crc     = *(uint32_t*)buffer;
                msg     = buffer + sizeof(crc);
                msg_len = read - sizeof(crc);
#else
                msg = buffer;
#endif
                printf("received: %s", msg);

                state = PROC;
                break;
            }
            case PROC: {

                r = NULL;
                // check CRC of the incoming message
                if (__begin(msg, msg_len, crc)) {
                    r =  ukv_recv(ukv, msg);

                    if (!r) 
                        state = FINI;
                    else {
                        // calculate CRC of the response message
                        __output_append(r, strlen(r));

                        // since the message is complete, finalize CRC
                        __output_done();

                        state = SEND;
                    }

                    __end();
                } else {
                    // the message is corrupted, drop it
                    state = CORR;
                }

                break;
            }
            case SEND: {
#ifndef SEI_DISABLED
                // read the calculated CRC and add it to the response
                uint32_t ocrc =  __crc_pop();
                msg_len = sizeof(uint32_t) + strlen(r);
                char response[msg_len];

                memcpy(response, &ocrc, sizeof(int));
                memcpy(response + sizeof(int), r, strlen(r));
                sendto(cfd, response, msg_len, 0,
                       (struct sockaddr*) &caddr, sizeof(caddr));
#else
                sendto(cfd, r, strlen(r), 0,
                       (struct sockaddr*) &caddr, sizeof(caddr));
#endif
                printf("replied: %s", r);

                __begin_nm();
                ukv_done(ukv, r);
                __end();
                printf("---------------------\n");
                
                state = RECV;
                break;
            }
            case CORR: {
                // if we received a corrupted message, send a response to the
                // client anyway
                char* reply;
                __begin_nm();
                reply = strdup("%error\r\n");
                if (!reply)
                    return EXIT_FAILURE;

                __output_append(reply, strlen(reply));
                __output_done();

                __end();
                printf("replied: %s", reply);
                printf("---------------------\n");

#ifndef SEI_DISABLED
                uint32_t ocrc =  __crc_pop();
                msg_len = sizeof(uint32_t) + strlen(reply);
                char response[msg_len];

                memcpy(response, &ocrc, sizeof(int));
                memcpy(response + sizeof(int), reply, strlen(reply));
                sendto(cfd, response, msg_len, 0,
                       (struct sockaddr*) &caddr, sizeof(caddr));
#else
                sendto(cfd, reply, strlen(reply), 0,
                       (struct sockaddr*) &caddr, sizeof(caddr));
#endif

                free(reply);

                state = RECV;
                break;
            }
            case FINI: 
                close(fd);
                __begin_nm();
                ukv_fini(ukv);
                __end();

                return 0;
        }
    }
}
