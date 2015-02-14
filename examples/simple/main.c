/* ----------------------------------------------------------------------
 * Copyright (c) 2015 Diogo Behrens, Sergei Arnautov
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */

#include <sei.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1024

/* counter represents the global state we wish to protect. All updates of the
 * state should be carried out within a hardened handler. */
size_t counter;

FILE *ifile = NULL, *ofile = NULL;
char* buffer;

/* library calls within event handlers need to be annotated with TMASCO_PURE to
 * instruct the compiler not to instrument them */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, 
				FILE *stream) TMASCO_PURE;
void exit(int status) TMASCO_PURE;
FILE *fopen(const char *path, const char *mode) TMASCO_PURE;
int fclose(FILE *fp) TMASCO_PURE;
int sprintf(char *str, const char *format, ...) TMASCO_PURE;

/* initialize the state */
void init_counter() {
	counter = 0;
}

/* extract crc from the message
 * to simplify the example we read input messages from a file */
size_t 
recv_msg_and_crc(char** imsg, uint32_t* crc) {
	if (ifile == NULL) {
		ifile = fopen("input", "r");

		if (ifile == NULL) {
			printf("could not open input file\n");
			exit(-1);
		}
	}

	size_t n;
	ssize_t read = getline(&buffer, &n, ifile);

	if (read < 0) {
		fclose(ifile);
		if (ofile)
			fclose(ofile);
		exit(0);
	}

	*crc  = *(uint32_t*)buffer;
	*imsg = buffer + sizeof(*crc);
	return read - sizeof(*crc);
}

/* process the input message: simply add message size to the counter */
void
do_something_here(const char* imsg) {
	counter += strlen(imsg);
}

/* create output message: counter value */
char*
create_a_message_here(size_t* olen) {
	char* msg = malloc(16);
	sprintf(msg, "counter=%zu\n", counter);
	*olen = strlen(msg);
	return msg;
}

/* send the message along with crc (write it to a file in this example) */
void
send_msg_and_crc(char* omsg, const size_t olen, const uint32_t crc) {
	if (ofile == NULL) {
		ofile = fopen("output", "w");

		if (ofile == NULL) {
			if (ifile)
				fclose(ifile);
			printf("cannot open output file\n");
			exit(-1);
		}
	}

	fwrite(&crc, sizeof(crc), 1, ofile); 
	fwrite(omsg, olen, 1, ofile);

	free(omsg);
	free(buffer);
	buffer = NULL;
}

int 
main(int argc, char** args) {
	char     *imsg = NULL, *omsg = NULL;
	uint32_t crc;
	size_t   ilen, olen;

	/* initialize the state using a hardened event handler which will be
	 * executed twice. Since no message is received, use __begin_nm - no message */
	__begin_nm();
	init_counter();
	__end_nm();

	while(1) {
		ilen = recv_msg_and_crc(&imsg, &crc);

		/* __begin checks if incoming message is correct
		 * must be called within if-statement   */
		if (__begin(imsg, ilen, crc)) { 

			/* process the message */
			do_something_here(imsg); 

			/* create output message */
			omsg = create_a_message_here(&olen);

			/* calculate CRC of the output message; if only a part of the 
			 * message is created, this function should be called again 
			 * for other parts of the message */
			__output_append(omsg, olen); 

			/* Finalize CRC once a complete output message was created */
			__output_done(); 

			/* end of the hardened event handler */
			__end();
		} else /* discard invalid input */ 
			continue; 
		
		printf("counter: %zu\n", counter);
		
		/* read the calculated CRC and send it along with the message */
		send_msg_and_crc(omsg, olen, __crc_pop());
	}

	return 0;
}
