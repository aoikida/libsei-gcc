/* -----------------------------------------------------------------------------
 * Copyright (c) 2015 Diogo Behrens, Sergei Arnautov
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 1024

int
main(const int argc, const char* argv[])
{

	if (argc < 3) {
		printf("usage: %s <server ip> <server port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int fd;
	struct sockaddr_in addr;
	int port;
	char* req = NULL;
	ssize_t read;
	char buffer[BUFSIZE+1];
	size_t l = 0;

	port = atoi(argv[2]);
	crc_init();

	// create socket and connect
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("failed to create a socket");
		return EXIT_FAILURE;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(port);
	
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("connect failed");
		return EXIT_FAILURE;
	}

	printf("ukv interface:\n\tset: +key,value\n\tget: ?key\n\tdel: -key\n\n");
	printf("type exit to quit\n\n");

	printf("enter command: \n");

	while ((read = getline(&req, &l, stdin)) != -1) {
		if (read > 1) { 
			if (strcmp(req, strdup("exit\n")) == 0) {
				free(req);
				exit(0);
			}	
			// the server expects \r\n in the end of the request
			// overwrite \n in the end and the terminating null
			req[read-1] = '\r';
			req[read]   = '\n';
			read++;

			// calculate CRC of the request
			uint32_t crc = crc_compute(req, read);
			 
			// preprend the request with CRC and send
			ssize_t len = read + sizeof(crc);
			char buf[len];
			memcpy(buf, &crc, sizeof(crc));
			memcpy(buf + sizeof(crc), req, read);
			sendto(fd,buf,len,0,(struct sockaddr *)&addr,sizeof(addr)); 

			// get response and check CRC
			len = recv(fd, buffer, BUFSIZE, 0);
			if (len < 0) {
				perror("receive failed");
			 	return EXIT_FAILURE;
			}

			buffer[len] = '\0';

			uint32_t icrc = *(uint32_t*)buffer;
			char* msg = buffer + sizeof(icrc);
			crc = crc_compute(msg, len-sizeof(icrc));

			if (icrc != crc) 
				printf("dropping corrupted response\n");
			else 
				printf("server response: %s", msg);

			printf("------------------------\n");

			free(req);
			req = NULL;
		}
	}
	return 0;
}
