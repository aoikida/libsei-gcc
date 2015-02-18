/* ----------------------------------------------------------------------------
 * Copyright (c) 2015 Diogo Behrens, Sergei Arnautov
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

int
main(const int argc, const char* argv[])
{

    FILE* fd;
    size_t l = 0;
    ssize_t read;
    char* req = NULL;
    int corrupt = 0;
    crc_init();

    fd = fopen("input", "w");
    if (fd < 0) {
        printf("couldn't open file for writing");
        return -1;
    }

    printf("if the first character in a line is !, CRC will be corrupted\n");
    printf("type exit to quit\n\n");

    while ((read = getline(&req, &l, stdin)) != -1) {
        if (read > 1) { 
            if (strcmp(req, strdup("exit\n")) == 0) {
                free(req);
                fclose(fd);
                exit(0);
            }   

            if (req[0] == '!')
                corrupt = 1;

            uint32_t crc = crc_compute(req + corrupt, read - corrupt);
            if (corrupt) 
                crc++;

            fwrite(&crc, sizeof(crc), 1, fd); 
            fwrite(req + corrupt, read - corrupt, 1, fd);
        }

        free(req);
        req = NULL;
        corrupt = 0;
    }

    fclose(fd);
    return 0;
}
