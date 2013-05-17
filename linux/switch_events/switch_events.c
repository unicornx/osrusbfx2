/*****************************************************************************/
/* switch_events.c  A Tests for OSR USB-FX2 Switch Event Notification        */
/*                                                                           */
/* Copyright (C) 2006 by Robin Callender                                     */
/*                                                                           */
/* This program is free software. You can redistribute it and/or             */
/* modify it under the terms of the GNU General Public License as            */
/* published by the Free Software Foundation, version 2.                     */
/*                                                                           */
/* returns 0  if switch event happened.                                      */
/* returns !0 if error condition.                                            */
/*                                                                           */
/* NOTES:                                                                    */
/*                                                                           */
/* The POLLPRI flag will signal a switch event on a poll request.            */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
                                                                        
int main( void ) 
{
    int    result;
    int    timeout = 5000;                 /* Timeout in msec: 5 sec. */
    char * devpath = "/dev/osrfx2_0";

    struct pollfd pollfds;

    /* 
     *   Initialize pollfds; get input from /dev/osrfx2_0 
     */
    pollfds.fd = open(devpath, O_RDWR);

    if (pollfds.fd  < 0) {
        printf("open failed: %d\n", pollfds.fd);
        exit (1);
    }

    pollfds.events = POLLPRI;      /* Wait for priority input */

    for (;;) {

        result = poll( &pollfds, 1, timeout ); 

        switch (result) {
        
            case 0:  /* timeout */
                break;
         
            case -1:                                                       
                exit(1);                                                    
         
            default:                                                      
                if (pollfds.revents & POLLPRI) {
                    exit(0);
                }
        }
    }

    close(pollfds.fd);

    return 0;
}
