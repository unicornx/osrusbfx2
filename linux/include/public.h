/**
 * public.h
 * 
 * osrfx2  - A Driver for the OSR USB FX2 Learning Kit device
 *
 * This program is free software. You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#ifndef _PUBLIC_H
#define _PUBLIC_H

/* Define these values to match your devices */
#define OSRFX2_VENDOR_ID	0x0547
#define OSRFX2_PRODUCT_ID	0x1002

/* Define the vendor commands supported by OSR USB FX2 device. */
#define OSRFX2_READ_7SEGMENT_DISPLAY      0xD4
#define OSRFX2_READ_SWITCHES              0xD6
#define OSRFX2_READ_BARGRAPH_DISPLAY      0xD7
#define OSRFX2_SET_BARGRAPH_DISPLAY       0xD8
#define OSRFX2_IS_HIGH_SPEED              0xD9
#define OSRFX2_REENUMERATE                0xDA
#define OSRFX2_SET_7SEGMENT_DISPLAY       0xDB

/**
 * BARGRAPH_STATE is a bit field structure with each bit corresponding 
 * to one of the bar graph on the OSRFX2 Development Board
 *
 * Modified this structure to adpator to CY001
 * Refer to icd.h of osrfx2fw "LED Bar Graph" section to get more details
 */
#define BARGRAPH_ON  (unsigned char)0x80
#define BARGRAPH_OFF (unsigned char)0x00

#define BARGRAPH_MAXBAR (unsigned char)4 // CY001 only have 4 LED bars availabe

struct bargraph_state {
    union {
        struct {
            /*
             * Individual bars (LEDs) starting from the top of the display.
             *
             * NOTE: The display has 10 LEDs, but the top two LEDs are not
             *       connected (don't light) and are not included here. 
             */
            unsigned char Bar1 : 1;
            unsigned char Bar2 : 1;
            unsigned char Bar3 : 1;
            unsigned char Bar4 : 1;
            unsigned char Bar5 : 1; // not used for CY001
            unsigned char Bar6 : 1; // not used for CY001
            unsigned char Bar7 : 1; // not used for CY001
            unsigned char Bar8 : 1; // used by CY001 as flag for light(1)/clear(0)
        };
        /*
         *  The state of all eight bars as a single octet.
         */
        unsigned char BarsOctet;
    };
} __attribute__ ((packed));

#endif /*_PUBLIC_H */

