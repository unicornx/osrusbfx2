/*-----------------------------------------------------------------------------
   File:      icd.h
   Contents:  Interface Control Description
              Specific for CY001 dev board only
              This document is used to describe the board interface. Driver
              writers and Application writers should refer this.
 $Archive:  $
 $Date:  $
 $ $

   Copyright (c) , All rights reserved

Revision History:
Author         Date       Tracking Number  Description of Change
--------------+----------+---------------+--------------------------------------
unicornx       June/2012  N/A             Initial version
unicornx       Mar/2013   N/A             Changed definition for Bar Graph

-----------------------------------------------------------------------------*/
#ifndef _ICD_H //Header entry
#define _ICD_H

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
 bit: | 7     | 6   | 5   | 4   | 3     | 2     | 1     | 0
 -----+-------+-----+-----+-----+-------+-------+-------+-------
 def: | state | n/a | n/a | n/a | bar-4 | bar-3 | bar-2 | bar-1

 bit 0 ~ 3 indicates the LED bar index, bit0 is bar1 and so on.
 bit 4 ~ 6 are not used due to CY001 only has 4 LED bars.
 bit 7 indicates light status, on or off, 1 is on, 0 is off. This bit is only used for set.

 Note: this definition is a bit differnet against OSRFX2. OSRFX2 uses 8 bit to
 represent 8 LED bars, for every bit, 1 means switch on the bar, 0 means switch off it.
 For CY001, we use bit 0 ~ 3 to flag which bar would be affected, and use bit 7 to
 flag the operation type - off or on.

 -----------------------------------------------------------------------------*/
#define BARGRAPH_ON  0x80
#define BARGRAPH_OFF 0x00


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

#endif // _ICD_H

