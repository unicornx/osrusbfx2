/**
 * This program is free software. You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * test app for osrfx2 driver.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <assert.h>

#include "public.h"

/* It's better to define a max length for myself instead of system defined.
   Refer to http://stackoverflow.com/questions/833291/is-there-an-equivalent-to-winapis-max-path-under-linux-unix
*/
#define MAX_DEVPATH_LENGTH 256

/*---------------------------------------------------------------------------*/
/* Global data                                                               */
/*---------------------------------------------------------------------------*/
#ifdef BOOL
#undef BOOL
#endif
#define BOOL int

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef ULONG
#undef ULONG
#endif
#define ULONG unsigned long


BOOL G_fDumpUsbConfig = FALSE;    // flags set in response to console command line switches
BOOL G_fDumpReadData = FALSE;
BOOL G_fRead = FALSE;
BOOL G_fWrite = FALSE;
BOOL G_fPlayWithDevice = FALSE;
BOOL G_fPerformBlockingIo = TRUE;
unsigned long G_IterationCount = 1; //count of iterations of the test we are to perform
int G_WriteLen = 512;         // #bytes to write
int G_ReadLen = 512;          // #bytes to read

char * gDevName = NULL;
char * gDevPath = NULL;
char * gSysPath = NULL;

typedef enum _INPUT_FUNCTION {
    LIGHT_ONE_BAR = 1,
    CLEAR_ONE_BAR,
    LIGHT_ALL_BARS,
    CLEAR_ALL_BARS,
    GET_BAR_GRAPH_LIGHT_STATE,
    GET_MOUSE_POSITION,
    GET_MOUSE_POSITION_AS_INTERRUPT_MESSAGE,
    GET_7_SEGEMENT_STATE,
    SET_7_SEGEMENT_STATE,
    RESET_DEVICE,
    REENUMERATE_DEVICE,
    GET_DEV_INFO,
} INPUT_FUNCTION;

/*
 Retrun 0, OK, else failed
*/
static int getDevicePath ( void )
{
	char * devname;
	char   devpath [MAX_DEVPATH_LENGTH];
	char   syspath [MAX_DEVPATH_LENGTH];

	devname = (gDevName) ? gDevName : "osrfx2_0";

	// get device path
	snprintf ( devpath, sizeof(devpath), "/dev/%s", devname );
	if( access( devpath, F_OK ) == -1 ) {
    	fprintf ( stderr, "Can't find %s device\n", devpath );
    	return -1;
	}
	gDevPath = strdup(devpath);

	// get sys path
	snprintf(syspath, sizeof(syspath), "/sys/class/usb/%s/device", devname);
	gSysPath = strdup(syspath);

	return 0;
}

/*++
Routine Description:
    Called by main() to dump usage info to the console when
    the app is called with no parms or with an invalid parm
Arguments:
    None
Return Value:
    None
--*/
void printUsage()
{
    printf("Usage for osrusbfx2 testapp:\n");
    printf("-r [n] where n is number of bytes to read\n");
    printf("-w [n] where n is number of bytes to write\n");
    printf("-c [n] where n is number of iterations (default = 1)\n");
    printf("-v verbose -- dumps read data\n");
    printf("-p to control bar LEDs, seven segment, and dip switch\n");
    printf("-n to perform non-blocking I/O (default is blocking I/O)\n");
    printf("-u to dump USB configuration and pipe info \n");

    return;
}

/*++
Routine Description:
    Called by main() to parse command line parms
Arguments:
    argc and argv that was passed to main()
Return Value:
	1, parse OK, 0, parse failed
    When parse OK, sets global flags as per user function request
--*/
int parseArg( int argc, char** argv )
{
    int ch;
    opterr=0;
    int retval = 1;

    if ( argc < 2 ) { // give usage if invoked with no parms
        printUsage();
        return 0;
    }

    /* Regarding getopt, refer to http://blog.csdn.net/lazy_tiger/article/details/1806367 */
  	while ( ( 1 == retval ) && 
  		    ( ( ch = getopt ( argc, argv, "r:R:w:W:c:C:uUpPnNvV" ) ) != -1 ) ) {
#if 0
    	printf("optind:%d\n",optind);
    	printf("optarg:%s\n",optarg);
    	printf("ch:%c\n",ch);
    	printf("optopt+%c\n",optopt);
#endif    	
    	switch(ch) {
        case 'r':
        case 'R':
			G_ReadLen = atoi ( optarg );
			if ( 0 == G_ReadLen ) {
				if ( 0 != strcmp ( "0", optarg ) ) {
					fprintf( stderr, "atoi\n" );
					retval = 0;
				}
			}
            if ( 1 == retval ) {
	            printf ( "r/R, read len = %d\n", G_ReadLen );
	            G_fRead = TRUE;
            }
            break;
            
        case 'w':
        case 'W':
			G_WriteLen = atoi ( optarg );
			if ( 0 == G_WriteLen ) {
				if ( 0 != strcmp ( "0", optarg ) ) {
					fprintf( stderr, "atoi\n" );
					retval = 0;
				}
			}
            if ( 1 == retval ) {
	            printf ( "w/W, write len = %d\n", G_WriteLen );
	            G_fWrite = TRUE;
            }
            break;        	
            
        case 'c':
        case 'C':
			G_IterationCount = atoi ( optarg );
			if ( 0 == G_IterationCount ) {
				if ( 0 != strcmp ( "0", optarg ) ) {
					fprintf( stderr, "atoi\n" );
					retval = 0;
				}
			}
            if ( 1 == retval ) {
            	printf ( "c/C, IterationCount = %ld\n", G_IterationCount );
	            G_fWrite = TRUE;
            }
            break;
            
        case 'u':
        case 'U':
            G_fDumpUsbConfig = TRUE;
            break;
            
        case 'p':
        case 'P':
            G_fPlayWithDevice = TRUE;
            break;
            
        case 'n':
        case 'N':
            G_fPerformBlockingIo = FALSE;
            break;
            
        case 'v':
        case 'V':
            G_fDumpReadData = TRUE;
            break;
            
        default:
            retval = 0;
	    }
	}

  	if ( 0 == retval ) printUsage();

  	return retval;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static char * getBargraphDisplay( void )
{
    int    fd;
    int    count;
    char   attrname [MAX_DEVPATH_LENGTH];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/bargraph", gSysPath);

    fd = open( attrname, O_RDONLY );
    if (fd == -1)  
        return NULL;

	memset ( attrvalue, 0x00, 32 );
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
    char   attrname [MAX_DEVPATH_LENGTH];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/bargraph", gSysPath);
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

#if 0
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int get7SegmentDisplay( unsigned char * value )
{
    int    fd;
    int    count;
    char   attrname [256];
    char   attrvalue [32];

    snprintf(attrname, sizeof(attrname), "%s/7segment", gSysPath);

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

    snprintf(attrname, sizeof(attrname), "%s/7segment", gSysPath);
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

    snprintf(attrname, sizeof(attrname), "%s/switches", gSysPath);

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
    pollfds.fd = open( gSysPath, O_RDWR );
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
#endif

void PlayWithDevice ()
{
    int function;
	int bar;
	unsigned char barValue;

    /*
     * Infinitely print out the list of choices, ask for input, process
     * the request.
     */
    while ( 1 ) {

		printf ("\nUSBFX TEST -- Functions:\n\n");
        printf ("\t1.  Light Bar\n");
        printf ("\t2.  Clear Bar\n");
        printf ("\t3.  Light entire Bar graph\n");
        printf ("\t4.  Clear entire Bar graph\n");
        printf ("\t5.  Get bar graph state\n");
        printf ("\t6.  Get Mouse position\n");
        printf ("\t7.  Get Mouse Interrupt Message\n");
        printf ("\t8.  Get 7 segment state\n");
        printf ("\t9.  Set 7 segment state\n");
        printf ("\t10. Reset the device\n");
        printf ("\t11. Reenumerate the device\n");
        printf ("\n\t0. Exit\n");
        printf ("\n\tSelection: ");
        
        if ( scanf ( "%d", &function ) <= 0 ) {
            printf("Error reading input!\n");
            goto Error;
        }

        switch ( function ) {

        case LIGHT_ONE_BAR: 
	    	printf ( "Which Bar (input number 1 thru %d)?\n", BARGRAPH_MAXBAR );
        	if ( scanf ( "%d", &bar ) <= 0 ) {
	            printf ( "Error reading input!\n" );
    	        goto Error;
	        }

			if ( bar == 0 || bar > BARGRAPH_MAXBAR ) {
	            printf ( "Invalid bar number!\n" );
	            goto Error;
	        }

        	bar--; // normalize to 0 to 3

        	barValue = 1 << (unsigned char)bar;
			barValue |= BARGRAPH_ON;

			if ( 0 != setBargraphDisplay ( barValue ) ) {
				printf ( "setBargraphDisplay failed with error \n" );
	            goto Error;	
			}

            break;

        case CLEAR_ONE_BAR:
			printf ( "Which Bar (input number 1 thru %d)?\n", BARGRAPH_MAXBAR );
			if ( scanf ("%d", &bar ) <= 0 ) {
	            printf ( "Error reading input!\n" );
    	        goto Error;
	        }

    	    if ( bar == 0 || bar > BARGRAPH_MAXBAR ) {
	            printf("Invalid bar number!\n");
    	        goto Error;
        	}

			bar--;
            barValue = BARGRAPH_OFF; // reset and flag off
			barValue |= 1 << (unsigned char) bar;

            if ( 0 != setBargraphDisplay ( barValue ) ) {
				printf ( "setBargraphDisplay failed with error \n" );
	            goto Error;	
			}
            
            break;

        case LIGHT_ALL_BARS:
            if ( 0 != setBargraphDisplay ( 0x8F ) ) {
				printf ( "setBargraphDisplay failed with error \n" );
	            goto Error;	
			}
            break;

        case CLEAR_ALL_BARS:
        	if ( 0 != setBargraphDisplay ( 0x0F ) ) {
				printf ( "setBargraphDisplay failed with error \n" );
	            goto Error;	
			}
            break;

		case GET_BAR_GRAPH_LIGHT_STATE:
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

            
       	case GET_MOUSE_POSITION:
       		printf("\nOSRFX2 -- NOT supported!\n");
       		break;

	    case GET_MOUSE_POSITION_AS_INTERRUPT_MESSAGE:
	    	printf("\nOSRFX2 -- to be done\n");
#if 0
            {
                if( waitForSwitchEvent() != 0) {
                    break;
                }
            }
            /* Note fall-through to case 5 */
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

#endif

        case GET_7_SEGEMENT_STATE:
			printf("\nOSRFX2 -- to be done\n");
#if 0            	
                if ( 0 != get7SegmentDisplay( &i ) ) {
                    goto Error;
                }
                printf ( "7-Segment Display value:  %c\n", i );
#endif                
            break;

        case SET_7_SEGEMENT_STATE: 
        	printf("\nOSRFX2 -- to be done\n");

#if 0            	
                for (i=0; i < 10; i++) {
                    retval = set7SegmentDisplay( i );
                    if (retval != 0) {
                        break;
                    }
                    sleep(1);
                }
#endif                
            break;

        case RESET_DEVICE:
			// TBD
			printf("\nOSRFX2 -- to be done\n");
            break;

        case REENUMERATE_DEVICE:
        	// TBD
        	printf("\nOSRFX2 -- to be done\n");
        	break;
        	
        default:
            printf("\nOSRFX2 -- done\n");
			goto Error;
        }
    } // end of while loop

Error:
	return;
}

#define NPERLN 8

void Dump ( unsigned char *b, int len )
/*++
Routine Description:

    Called to do formatted ascii dump to console of the io buffer

Arguments:

    buffer and length

Return Value:

    none

--*/
{
    ULONG i;
    ULONG longLen = (ULONG)len / sizeof( ULONG );
    ULONG *pBuf = (ULONG*) b;

    // dump an ordinal ULONG for each sizeof(ULONG)'th byte
    printf("\n****** BEGIN DUMP LEN decimal %d, 0x%x\n", len,len);
    for (i=0; i<longLen; i++) {
        printf("%04X ", (ULONG)(*pBuf++));
        if (i % NPERLN == (NPERLN - 1)) {
            printf("\n");
        }
    }
    if (i % NPERLN != 0) {
        printf("\n");
    }
    printf("\n****** END DUMP LEN decimal %d, 0x%x\n", len,len);
}

void ReadWrite ( )
{
    int wfd = -1; // write file descriptor
    int rfd = -1; // read file descriptor

    unsigned char * pInBuf = NULL;
    unsigned char * pOutBuf = NULL;
    ssize_t wlen;
    ssize_t rlen;

    int i;

    if (G_fRead) {
        if ( G_fDumpReadData ) { // round size to sizeof ULONG for readable dumping
            while( G_ReadLen % sizeof( ULONG ) ) {
                G_ReadLen++;
            }
        }
    }

    if (G_fWrite) {
        if ( G_fDumpReadData ) { // round size to sizeof ULONG for readable dumping
            while( G_WriteLen % sizeof( ULONG ) ) {
                G_WriteLen++;
            }
        }
    }

    wfd = open ( gDevPath, O_WRONLY | O_NONBLOCK );
    if ( -1 == wfd ) {
        fprintf ( stderr, "open for write: %s failed\n", gDevPath );
        goto exit;
    }

    rfd = open ( gDevPath, O_RDONLY | O_NONBLOCK );
    if ( -1 == rfd ) {
        fprintf ( stderr, "open for read: %s failed\n", gDevPath );
        goto exit;
    }

	pInBuf = malloc(G_ReadLen);
	if ( NULL == pInBuf ) goto exit;
    
    pOutBuf = malloc(G_WriteLen);
    if ( NULL != pOutBuf ) {
	    ULONG *pOut = (ULONG *) pOutBuf;
    	ULONG numLongs = G_WriteLen / sizeof( ULONG );
	    /* put some data in the output buffer */
    	int j;
    	for ( j=0; j<numLongs; j++ ) {
	        *(pOut+j) = j;
    	}
    }
    else goto exit;

    for ( i = 0; i < G_IterationCount; i++ ) {

        if ( G_fWrite ) {
            //
            // send the write
            //
			wlen = write ( wfd, pOutBuf, G_WriteLen );
            if ( wlen < 0 ) {
                fprintf ( stderr, "write error\n");
                goto exit;
            }
            printf ("write (%04.4d) : request %06.6d bytes -- %06.6d bytes written\n",
                    i, G_WriteLen, wlen );
            assert ( wlen == G_WriteLen );
        }

        if ( G_fRead ) {

            int index = 0;

            do {
                rlen = read ( rfd, &pInBuf[index], (G_ReadLen - index) );
                if ( rlen < 0 ) {
                    fprintf ( stderr, "read error\n" );
                    goto exit;
                }
                printf("Read (%04.4d)(%04.4d) : request %06.6d bytes -- %06.6d bytes read\n",
                   i, index, (G_ReadLen - index), rlen );

                index += rlen;

            } while ( index < G_ReadLen );
            assert ( index == G_ReadLen );

            if (G_fWrite) {

                /* validate the input buffer against what
                 * we sent
                 * Till we arrive here, we have asserted length of
                 * read should be G_ReadLen and that of write should
               	 * be G_WriteLen
               	 */
                if ( memcmp ( pOutBuf, pInBuf, G_WriteLen ) != 0 ) {
                	fprintf ( stderr, "Mismatch error between buffer contents.\n" );
	            }

                if( G_fDumpReadData ) {
                    printf("Dumping read buffer\n");
                    Dump( pInBuf,  G_ReadLen );
                    printf("Dumping write buffer\n");
                    Dump( pOutBuf, G_WriteLen );
                }
            }
        }
    }



exit:
	if ( wfd > 0 ) close ( wfd );
    if ( rfd > 0 ) close ( rfd );

	if ( NULL != pInBuf ) free ( pInBuf );
	if ( NULL != pOutBuf ) free ( pOutBuf );
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
int main ( int argc, char ** argv )
{
    int           retval = 0;

	if ( 0 == parseArg ( argc, argv ) ) {
		retval = -1;
		goto done;
	}

	if ( 0 != getDevicePath() ) {
		retval = -1;
        goto done;
    }

	/* start process */

	// play
    if (G_fPlayWithDevice) {
        PlayWithDevice();
        goto done;
    }

    // doing a read, write, or both test
    if ((G_fRead) || (G_fWrite)) {
    	ReadWrite ( );
    }

    /*
     *   Clean-up and exit.
     */ 
done:
    if (gDevName)   free(gDevName);
    if (gDevPath)   free(gDevPath);
    if (gSysPath)   free(gSysPath);

    return retval;
}

