/*---------------------------------------------------------------------------*/
/* osrfx2.c - OSR USB FX2 Learning Kit application for Linux                 */
/*                                                                           */
/* Copyright (C) 2006 by Robin Callender                                     */
/*                                                                           */
/* This program is free software. You can redistribute it and/or             */
/* modify it under the terms of the GNU General Public License as            */
/* published by the Free Software Foundation, version 2.                     */
/*---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/poll.h>

/*---------------------------------------------------------------------------*/
/* Global data                                                               */
/*---------------------------------------------------------------------------*/
char * gDevname = NULL;
char * gSyspath = NULL;

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static char * getBargraphDisplay( void )
{
    int    fd;
    int    count;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/bargraph", gSyspath);

    fd = open( attrname, O_RDONLY );
    if (fd == -1)  
        return NULL;

    count = read( fd, &attrvalue, sizeof(attrvalue) );
     close(fd);
    if (count == 8) { 
        return strdup(attrvalue);
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int setBargraphDisplay( unsigned char value )
{
    int    fd;
    int    count;
    int    len;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/bargraph", gSyspath);
    snprintf(attrvalue, sizeof(attrvalue), "%d", value);
    len = strlen(attrvalue) + 1;

    fd = open( attrname, O_WRONLY );
    if (fd == -1) 
        return -1;

    count = write( fd, &attrvalue, len );
    close(fd);
    if (count != len) 
        return -1;

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int get7SegmentDisplay( unsigned char * value )
{
    int    fd;
    int    count;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/7segment", gSyspath);

    fd = open( attrname, O_RDONLY );
    if (fd == -1)  
        return -1;

    count = read( fd, &attrvalue, sizeof(attrvalue) );
    close(fd);
    if (count > 0) { 
        *value = attrvalue[0];
        return 0;
    }
    return -1;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int set7SegmentDisplay( unsigned char value )
{
    int    fd;
    int    count;
    int    len;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/7segment", gSyspath);
    snprintf(attrvalue, sizeof(attrvalue), "%d", value);
    len = strlen(attrvalue) + 1;

    fd = open( attrname, O_WRONLY );
    if (fd == -1) 
        return -1;

    count = write( fd, &attrvalue, len );
    close(fd);
    if (count != len) 
        return -1;

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static char * getSwitchesState( void )
{
    int    fd;
    int    count;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/switches", gSyspath);

    fd = open( attrname, O_RDONLY );
    if (fd == -1)  
        return NULL;

    count = read( fd, &attrvalue, sizeof(attrvalue) );
    close(fd);
    if (count == 8) { 
        return strdup(attrvalue);
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int waitForSwitchEvent( void ) 
{
    int    result;
    int    timeout = 5000;     /* Timeout in msec: 5 sec. */
    struct pollfd pollfds;

    /* 
     *   Initialize pollfds; get input from /dev/osrfx2_0 
     */
    pollfds.fd = open( gSyspath, O_RDWR );
    if (pollfds.fd  < 0) {
        return -1;
    }
    pollfds.events = POLLPRI;      /* Wait for priority input */

    for (;;) {
        result = poll( &pollfds, 1, timeout ); 
        switch (result) {
            case 0:  /* timeout */
                break;
            case -1:                                                       
                return -1;
            default:                                                      
                if (pollfds.revents & POLLPRI) {
                    close(pollfds.fd);
                    return 0;
                }
        }
    }
    close(pollfds.fd);
    return -1;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void get_osrfx2_syspath( void )
{
    int fd;
	char * devname;
	char   devpath [256];
	char   syspath [256];

	devname = (gDevname) ? gDevname : "osrfx2_0";

	snprintf(devpath, sizeof(devpath), "/dev/%s", devname);

	fd = open(devpath, O_RDONLY);
	if (fd == -1){
        fprintf(stderr, "Can't find %s device\n", devpath);
		return;
	}
	close(fd);
	
	snprintf(syspath, sizeof(syspath), "/sys/class/usb/%s/device", devname);
	
	gSyspath = strdup(syspath);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
int main(int argc, char ** argv)
{
    extern char * optarg;
    extern int    optind;
    int           ch;
    int           retval = -1;

    /* 
     *   Capture any command-line args.
     */
    while ((ch = getopt(argc, argv, "d:h")) != -1) {
        switch (ch) {
        case 'd':
            gDevname = strdup(optarg);
            break;
        case 'h':
        default:
            fprintf(stderr, "Usage:\n" );
            fprintf(stderr, "   -d: device name to use: osrfx2_[0-9]\n");
            goto done;
        }
    }

	/* 
     *  Get the path to osrfx2's sysfs attributes.
	 */
    get_osrfx2_syspath();
    if (gSyspath == NULL) {
        goto done;
    }

    /*
     *  Enter command processing loop
     */
    for (;;) {

        int selection;
        unsigned char i;

        printf ("\nOSRFX2 -- Functions:\n\n");
        printf ("\t1.  Light bars\n");
        printf ("\t2.  Light entire bar graph\n");
        printf ("\t3.  Clear entire bar graph\n");
        printf ("\t4.  Get bar graph state\n");
        printf ("\t5.  Get switch state\n");
        printf ("\t6.  Get switch interrupt message\n");
        printf ("\t7.  Get 7-segment display\n");
        printf ("\t8.  Set 7-segment display\n");
        printf ("\n\t0. Exit\n");
        printf ("\n\tSelection: ");

        if (scanf ("%d", &selection) <= 0) {
            printf("Error reading input!\n");
            goto done;
        }

        switch (selection) {

        case 1: 
            {
                for (i=0; i < 8; i++) {
                    retval = setBargraphDisplay( 1<<i );
                    if (retval != 0) {
                        break;
                    }
                    sleep(1);
                }
            }
            break;

        case 2:
            {
                setBargraphDisplay( 0xFF );
            }
            break;

        case 3:
            {
                setBargraphDisplay( 0x00 );
            }
            break;

        case 4:
            {
                char * str = getBargraphDisplay();
                if (str) {
                    printf("Bar Graph: \n");
                    printf("    Bar8 is %s\n", str[7] == '*' ? "ON" : "OFF");
                    printf("    Bar7 is %s\n", str[6] == '*' ? "ON" : "OFF");
                    printf("    Bar6 is %s\n", str[5] == '*' ? "ON" : "OFF");
                    printf("    Bar5 is %s\n", str[4] == '*' ? "ON" : "OFF");
                    printf("    Bar4 is %s\n", str[3] == '*' ? "ON" : "OFF");
                    printf("    Bar3 is %s\n", str[2] == '*' ? "ON" : "OFF");
                    printf("    Bar2 is %s\n", str[1] == '*' ? "ON" : "OFF");
                    printf("    Bar1 is %s\n", str[0] == '*' ? "ON" : "OFF");

                    free(str);
                }
            }
            break;

            
       case 6:
            {
                if( waitForSwitchEvent() != 0) {
                    break;
                }
            }
            /* Note fall-through to case 5 */

        case 5:
            {
                char * str = getSwitchesState();
                if (str) {
                    printf("Switches: \n");
                    printf("    Switch8 is %s\n", str[7] == '*' ? "ON" : "OFF");
                    printf("    Switch7 is %s\n", str[6] == '*' ? "ON" : "OFF");
                    printf("    Switch6 is %s\n", str[5] == '*' ? "ON" : "OFF");
                    printf("    Switch5 is %s\n", str[4] == '*' ? "ON" : "OFF");
                    printf("    Switch4 is %s\n", str[3] == '*' ? "ON" : "OFF");
                    printf("    Switch3 is %s\n", str[2] == '*' ? "ON" : "OFF");
                    printf("    Switch2 is %s\n", str[1] == '*' ? "ON" : "OFF");
                    printf("    Switch1 is %s\n", str[0] == '*' ? "ON" : "OFF");
                    free(str);
                }
            }
            break;

        case 7: 
            {
                retval = get7SegmentDisplay( &i );
                if (retval != 0) {
                    break;
                }
                printf("7-Segment Display value:  %c\n", i);
            }
            break;

        case 8:
            {
                for (i=0; i < 10; i++) {
                    retval = set7SegmentDisplay( i );
                    if (retval != 0) {
                        break;
                    }
                    sleep(1);
                }
            }
            break;

        case 0:
            printf("\nOSRFX2 -- done\n");
            retval = 0;
			goto done;
        
        default:
            break;
        }
    }

    /*
     *   Clean-up and exit.
     */ 
done:
    if (gDevname)   free(gDevname);
    if (gSyspath)   free(gSyspath);

    return retval;
}

