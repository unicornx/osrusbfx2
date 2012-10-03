/*-----------------------------------------------------------------------------
   File:      icd.h
   Contents:  Interface Control Description
              Specific for CY001 dev board only
              This document is used to describe the board interface. Driver
              writers and application writers should refer this.
 $Archive:  $
 $Date:  $
 $ $

   Copyright (c) , All rights reserved
-----------------------------------------------------------------------------*/
#ifndef ICD_H //Header entry
#define ICD_H

//-----------------------------------------------------------------------------
// Define the vendor commands supported by our device
//
#define USBFX2LK_READ_7SEGMENT_DISPLAY      0xD4
#define USBFX2LK_READ_SWITCHES              0xD6
#define USBFX2LK_READ_BARGRAPH_DISPLAY      0xD7
#define USBFX2LK_SET_BARGRAPH_DISPLAY       0xD8
#define USBFX2LK_IS_HIGH_SPEED              0xD9
#define USBFX2LK_REENUMERATE                0xDA
#define USBFX2LK_SET_7SEGMENT_DISPLAY       0xDB
#define USBFX2LK_READ_DEVINFO_LEN           0xDC
#define USBFX2LK_READ_DEVINFO_DATA          0xDD


/*-----------------------------------------------------------------------------
	LED Bar Graph
	For OSRFX2 learning kit, there are 8 LED bars, but for CY001, it only has 4
  
	To set the bars, use 8-bits to set 4 LED bars state (light on/off)
  bit 7 ~ 4 indicates the LED bar index, bit-7 is bar-1 and so on
  bit 0 indicates light on or off, 1 is on, 0 is off
  
	To get the bars state, another 8-bits is used, format is the same as OSRFX2 board
  bit-3 ~ bit-0 represents bar-4 ~ bar-1
-----------------------------------------------------------------------------*/
#define BARGRAPH_SET_FLAGLIGHT 0x01
#define BARGRAPH_SET_FLAGCLEAR 0x00

#define BARGRAPH_SET_LED1_ON  0x80
#define BARGRAPH_SET_LED2_ON  0x40
#define BARGRAPH_SET_LED3_ON  0x20
#define BARGRAPH_SET_LED4_ON  0x10

/*-----------------------------------------------------------------------------
  Mouse Position tracking simulation
  CY001 board doesn't have switch, so we use the 4 buttons to simulate mouse moving track
  and demo how it works for the interrupit endpoint.
  Same as CY001, use four buttons/Keys to simulate mouse moving (UP, DOWN, LEFT, RIGHT)
  Use 8-bits BYTE to indicate the mouse moving actions
-----------------------------------------------------------------------------*/
#define MOUSEMOV_UP    0x01
#define MOUSEMOV_DOWN  0x02
#define MOUSEMOV_LEFT  0x04
#define MOUSEMOV_RIGHT 0x08

#endif // ICD_H
