/*****************************************************************************/
/* osrbulk.c    A Bulk Transfer Program for Testing the osrfx2 Driver        */
/*                                                                           */
/* Copyright (C) 2006 Robin Callender                                        */
/*                                                                           */
/* This program is free software. You can redistribute it and/or             */
/* modify it under the terms of the GNU General Public License as            */
/* published by the Free Software Foundation, version 2.                     */
/*                                                                           */
/* To run: ./osrbulk -t 2048 -c 1024 (for example)                           */
/*                                                                           */
/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/poll.h>

#undef TRUE
#define TRUE  (1)
#undef FALSE
#define FALSE (0)

/*****************************************************************************/
/* The options structure captures the command line options.                  */
/*****************************************************************************/
struct options {
    int  write_len;
    int  read_len;
    int  iterations;
    int  show_data;
};

/*****************************************************************************/
/* Show this program's usage summary.                                        */
/*****************************************************************************/
void usage()
{
    printf("Usage for Read/Write test:\n");
    printf("-t [n] where n is number of bytes to read and write\n");
    printf("-c [n] where n is number of iterations (default = 1)\n");
    printf("-v verbose -- dumps read data\n");

    exit(-1);
}

/*****************************************************************************/
/* Dump a block of data to stdout.                                           */
/*****************************************************************************/
void dump_data( unsigned char * buffer, unsigned int length)
{
   unsigned int    i;
   unsigned int    wholelines;
   unsigned char * current = buffer;
 
   if (length == 0) {
       printf("\n");
       return;
   }
 
   wholelines = length / 16;
 
   for (i=0; i < wholelines; i++) {
      printf("%05X: %02X %02X %02X %02X %02X %02X %02X %02X-"
                   "%02X %02X %02X %02X %02X %02X %02X %02X  "
                   "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
                (current-buffer),
                current[0], current[1], current[2], current[3],
                current[4], current[5], current[6], current[7],
                current[8], current[9], current[10],current[11],
                current[12],current[13],current[14],current[15],
                isprint(current[0])  ? current[0] : '.',
                isprint(current[1])  ? current[1] : '.',
                isprint(current[2])  ? current[2] : '.',
                isprint(current[3])  ? current[3] : '.',
                isprint(current[4])  ? current[4] : '.',
                isprint(current[5])  ? current[5] : '.',
                isprint(current[6])  ? current[6] : '.',
                isprint(current[7])  ? current[7] : '.',
                isprint(current[8])  ? current[8] : '.',
                isprint(current[9])  ? current[9] : '.',
                isprint(current[10]) ? current[10] : '.',
                isprint(current[11]) ? current[11] : '.',
                isprint(current[12]) ? current[12] : '.',
                isprint(current[13]) ? current[13] : '.',
                isprint(current[14]) ? current[14] : '.',
                isprint(current[15]) ? current[15] : '.' );
 
      current += 16;
      length  -= 16;
   }
 
   if (length) {
      printf("%05X: ", (current-buffer));
      for (i=0; i<length; i++) {
         printf("%02X ", current[i]);
      }
 
      for (; i < 16; i++) printf("   "); printf(" ");
 
      for (i=0; i < length; i++) {
         printf("%c", isprint(current[i]) ? current[i] : '.');
      }
      printf("\n");
   }
   else
      printf("\n");

   return;
}

/*****************************************************************************/
/* Parse the command line options and save them in the options structure.    */
/*****************************************************************************/
void parse_options(struct options * ops, int argc, char ** argv)
{
    int i;

    if (argc <= 1) 
        usage();

    for (i=0; i < argc; i++) {
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            switch (argv[i][1]) {
                 case 't':
                 case 'T':
                     ops->read_len = atoi(&argv[i+1][0]);
                     ops->write_len = ops->read_len;
                     i++;
                     break;
                 case 'c':
                 case 'C':
                     ops->iterations = atoi(&argv[i+1][0]);
                     i++;
                     break;
                 case 'v':
                 case 'V':
                     ops->show_data = TRUE;
                     i++;
                     break;
                 default:
                     usage();
            }
        }
    }
}

/*****************************************************************************/
/* Main - open a write and a read fd, then write and read from each.         */
/*****************************************************************************/
int main(int argc, char ** argv)
{
    int wfd = -1;
    int rfd = -1;
    ssize_t wlen;
    ssize_t rlen;
    unsigned char * buf_w = NULL;
    unsigned char * buf_r = NULL;

    char * devpath = "/dev/osrfx2_0";
    unsigned char gen = 0;
    int i, j;
    int retval = -1;

    struct options ops = {
        .write_len   = 0,
        .read_len    = 0,
        .iterations  = 1,
        .show_data   = FALSE,
    };

    parse_options(&ops, argc, argv);

    wfd = open(devpath, O_WRONLY | O_NONBLOCK);
    if (wfd == -1) {
        fprintf(stderr, "open for write: %s failed\n", devpath);
        goto error;
    }

    rfd = open(devpath, O_RDONLY | O_NONBLOCK);
    if (rfd == -1) {
        fprintf(stderr, "open for read: %s failed\n", devpath);
        goto error;
    }

    buf_w = malloc(ops.write_len);
    buf_r = malloc(ops.read_len);
    
    if (buf_w == NULL || buf_r == NULL) {
        fprintf(stderr, "memory allocation failed\n");
        goto error;
    }

    for (i=0; i < ops.iterations; i++) {

        /*
         *  Do the write-side operation.
         */
        {
            for (j=0; j < ops.write_len; j++) {
                buf_w[j] = gen;
                gen++;
            }

            wlen = write( wfd, buf_w, ops.write_len );
            if (wlen < 0) {
                fprintf(stderr, "write error\n");
                goto error;
            }
        }

        /*
         *  Do the read-side operation.
         */
        {
            struct pollfd pollfds;
            int timeout = 1000;      /* Timeout in msec: 1 sec. */
            int index;

            memset(buf_r, 0, ops.read_len);
            index = 0;               
            
            pollfds.fd = rfd;
            pollfds.events = POLLIN;

            do {

                switch ( poll(&pollfds, 1, timeout) ) {

                    case 0:           /* timeout: restart poll request */
                        continue;  

                    case -1:          /* error: what to do?  FIXME */
                        break;

                    default:
                        if ((pollfds.revents & POLLIN) == POLLIN) {

                            rlen = read( rfd, &buf_r[index], 
                                         (ops.read_len - index) );
                            if (rlen < 0) {
                                fprintf(stderr, "read error (%d)\n", rlen);
                                goto error;
                            }
                            index += rlen;
                        }
                        break;
                }

            } while ( index < ops.read_len );
        }

        if (ops.show_data == TRUE) {
            dump_data(buf_r, ops.read_len);
        }

        if (memcmp(buf_w, buf_r, ops.write_len) != 0) {
            fprintf(stderr, "Mismatch error between buffer contents.\n");
        }
    }

    retval = 0;  /* successful completion */

error:
    if (buf_w) free(buf_w);
    if (buf_r) free(buf_r);

    if (wfd > 0) close(wfd);
    if (rfd > 0) close(rfd);

    return retval;
}

