//-----------------------------------------------------------------------------
//   File:      bulkloop.c
//   Contents:  Hooks required to implement USB peripheral function.
//
// $Archive: /USB/Examples/FX2LP/bulkloop/bulkloop.c $
// $Date: 3/23/05 2:55p $
// $Revision: 4 $
//
//
//-----------------------------------------------------------------------------
// Copyright 2003, Cypress Semiconductor Corporation
//-----------------------------------------------------------------------------
#pragma NOIV               // Do not generate interrupt vectors

#include "fx2.h"
#include "fx2regs.h"
#include "syncdly.h"            // SYNCDELAY macro

#include "FX2LPSerial.h"

#include "icd.h"
#include "periph.h"

extern BOOL GotSUD;             // Received setup data flag
extern BOOL Sleep;
extern BOOL Rwuen;
extern BOOL Selfpwr;

BYTE Configuration;             // Current configuration
BYTE AlternateSetting;          // Alternate settings

#define VR_NAKALL_ON    0xD0
#define VR_NAKALL_OFF   0xD1

#define EP0BUFF_SIZE	0x40

/*-----------------------------------------------------------------------------
  Mouse Position tracking simulation
-----------------------------------------------------------------------------*/
BYTE xdata KeyS; // Key State, real-time key state. Board produces key actions 
                 // (UP, DOWN, LEFT, RIGHT) and report this to host through EP1

void MP_Init ( void )
{
	KeyS = 0x0;
}


/*-----------------------------------------------------------------------------
	Bar Graph Process
-----------------------------------------------------------------------------*/
BYTE xdata curLEDs; // used to record latest LED bars state set by host

void BG_Init ( void )
{
	curLEDs = 0;
}

// Set Bar Graph Display
void BG_Set ( void )
{
	EP0BCH=0;
	SYNCDELAY;
	EP0BCL=1;
	SYNCDELAY;

	EZUSB_Delay(100); // have to set delay here to light

	if ( EP0BUF[0] & BARGRAPH_SET_FLAGLIGHT )// ON
	{
		IOD = IOD & ( ~(EP0BUF[0] & 0xF0) ); // upper 4Pin
	}
	else   // OFF
	{	
		IOD = IOD | ( EP0BUF[0] & 0xF0 ); //  below 4Pin
	}

	curLEDs = ~(IOD >> 4);
	curLEDs &= 0xF;

	return;
}

void BG_Get ( void )
{
	EP0BUF[0] = curLEDs;

	EP0BCH=0;
	SYNCDELAY;
	EP0BCL=1;
	SYNCDELAY;

	return;
}

/*-----------------------------------------------------------------------------
	Device Information
-----------------------------------------------------------------------------*/
BYTE xdata devInfo[] = "SW version is: 1.0.0.0\n"
                       "Flex version is: 1.0.0.0\n"
                       "IMEI is: 1234567890\n";
#define LEN_DEVINFO 68

/*-----------------------------------------------------------------------------
	End Points
-----------------------------------------------------------------------------*/
void EP_Init ( void )
{
  // Registers which require a synchronization delay, see section 15.14
  // FIFORESET        FIFOPINPOLAR
  // INPKTEND         OUTPKTEND
  // EPxBCH:L         REVCTL
  // GPIFTCB3         GPIFTCB2
  // GPIFTCB1         GPIFTCB0
  // EPxFIFOPFH:L     EPxAUTOINLENH:L
  // EPxFIFOCFG       EPxGPIFFLGSEL
  // PINFLAGSxx       EPxFIFOIRQ
  // EPxFIFOIE        GPIFIRQ
  // GPIFIE           GPIFADRH:L
  // UDMACRCH:L       EPxGPIFTRIG
  // GPIFTRIG
  
  // Note: The pre-REVE EPxGPIFTCH/L register are affected, as well...
  //      ...these have been replaced by GPIFTC[B3:B0] registers

  // default: all endpoints have their VALID bit set
  // default: TYPE1 = 1 and TYPE0 = 0 --> BULK  
  // default: EP2 and EP4 DIR bits are 0 (OUT direction)
  // default: EP6 and EP8 DIR bits are 1 (IN direction)
  // default: EP2, EP4, EP6, and EP8 are double buffered

  // we are just using the default values, yes this is not necessary...
  // ep1: interrupt, IN from board to host
  EP1OUTCFG = 0xB0;
  EP1INCFG = 0xB0;
  
  SYNCDELAY;                    // see TRM section 15.14
  
  // ep6: bulk, OUT, maxPacketSize 64 bytes in full-speed, 512 bytes in high-speed
  EP6CFG = 0xA2;
  
  SYNCDELAY;                    

  // ep8: bulk, IN, maxPacketSize 64 bytes in full-speed, 512 bytes in high-speed  
  EP8CFG = 0xE0;

  SYNCDELAY;

  EP2CFG = EP4CFG = 0; // not used
  
  // out endpoints do not come up armed
  
  // since the defaults are double buffered we must write dummy byte counts twice
  SYNCDELAY;                    
  EP6BCL = 0x80;                // arm EP6OUT by writing byte count w/skip.
  SYNCDELAY;                    
  EP6BCL = 0x80;
  
  // enable dual autopointer feature
  AUTOPTRSETUP |= 0x01;
}

//-----------------------------------------------------------------------------
// Task Dispatcher hooks
//   The following hooks are called by the task dispatcher.
//-----------------------------------------------------------------------------

void TD_Init(void)             // Called once at startup
{
  BREAKPT &= ~bmBPEN; 
  Rwuen = TRUE;

  // set the CPU clock to 48MHz
  //CPUCS = ((CPUCS & ~bmCLKSPD) | bmCLKSPD1) ;	// Commented since the CPU frequency is configured in FX2LPSerial_Init()
  FX2LPSerial_Init(); 					// Serial Debug Code Start
  printf ( "Serial port initialized\r\n" );

  // set the slave FIFO interface to 48MHz
  IFCONFIG |= 0x40;

  BG_Init();

	MP_Init();

  OEA=0xF0;	  // the high 4 bits for output, low 4 bits for input.
  OED=0xFF;	  // all for output
  OEB=0x0;
  IOD=0xF0;	  // LEDs off; 7-Seg off.

	EP_Init();
}


void TD_Poll(void)              // Called repeatedly while the device is idle
{
  WORD i;
  WORD count;

	// Poll Key State
  if( !(EP1INCS & bmEPBUSY) ) // check if EP1 is ready
  {
		// Get curernt Key State on board
  	OEA=0xF0;// low 2 bits	 
		OEB=0xF0;// low 4 bits		
		KeyS=(~IOA&0x3)*0x10 + (~IOB&0x0F); 
	
	  if ( KeyS != 0 ) // have keys pressed
	  {
			EP1INBUF[0] = KeyS; // arm to EP1 IN buffer
			EP1INBC = 1; 		    // shoot
	  }      
  }		   

  // EP6 -> EP8
  if(!(EP2468STAT & bmEP6EMPTY))
  { // check EP6 EMPTY(busy) bit in EP2468STAT (SFR), core set's this bit when FIFO is empty
     if(!(EP2468STAT & bmEP8FULL))
     {  // check EP8 FULL(busy) bit in EP2468STAT (SFR), core set's this bit when FIFO is full
        APTR1H = MSB( &EP6FIFOBUF );
        APTR1L = LSB( &EP6FIFOBUF );

        AUTOPTRH2 = MSB( &EP8FIFOBUF );
        AUTOPTRL2 = LSB( &EP8FIFOBUF );

        count = (EP6BCH << 8) + EP6BCL;

        // loop EP6OUT buffer data to EP8IN
        for( i = 0x0000; i < count; i++ )
        {
           // setup to transfer EP6OUT buffer to EP8IN buffer using AUTOPOINTER(s)
           EXTAUTODAT2 = EXTAUTODAT1;
        }
        EP8BCH = EP6BCH;  
        SYNCDELAY;  
        EP8BCL = EP6BCL;        // arm EP6IN
        SYNCDELAY;                    
        EP6BCL = 0x80;          // re(arm) EP6OUT
     }
  }
}

BOOL TD_Suspend(void)          // Called before the device goes into suspend mode
{
   return(TRUE);
}

BOOL TD_Resume(void)          // Called after the device resumes
{
	BYTE temp;

	// recover 7-LEDs, display 'A' {'A', 0xF5},
	IOD = IOD & 0xF0;
	IOD += 0xF5 & 0x0F;
	IOA = ( IOA & 0xF ) + ( 0xF5 & 0xF0 );

	// recover LED bars
	temp = ( curLEDs << 4 ) & 0xF0;
	IOD = IOD & ( ~temp ); // upper 4Pin

	return(TRUE);
}

//-----------------------------------------------------------------------------
// Device Request hooks
//   The following hooks are called by the end point 0 device request parser.
//-----------------------------------------------------------------------------

BOOL DR_GetDescriptor(void)
{
   return(TRUE);
}

BOOL DR_SetConfiguration(void)   // Called when a Set Configuration command is received
{
   Configuration = SETUPDAT[2];
   return(TRUE);            // Handled by user code
}

BOOL DR_GetConfiguration(void)   // Called when a Get Configuration command is received
{
   EP0BUF[0] = Configuration;
   EP0BCH = 0;
   EP0BCL = 1;
   return(TRUE);            // Handled by user code
}

BOOL DR_SetInterface(void)       // Called when a Set Interface command is received
{
   AlternateSetting = SETUPDAT[2];
   return(TRUE);            // Handled by user code
}

BOOL DR_GetInterface(void)       // Called when a Set Interface command is received
{
   EP0BUF[0] = AlternateSetting;
   EP0BCH = 0;
   EP0BCL = 1;
   return(TRUE);            // Handled by user code
}

BOOL DR_GetStatus(void)
{
   return(TRUE);
}

BOOL DR_ClearFeature(void)
{
   return(TRUE);
}

BOOL DR_SetFeature(void)
{
   return(TRUE);
}

BOOL DR_VendorCmnd(void)
{
  BYTE tmp;
	WORD lenTotal, lenSent, i;
	BYTE *addr;
  
  switch (SETUPDAT[1])
  {
     case VR_NAKALL_ON:
        tmp = FIFORESET;
        tmp |= bmNAKALL;      
        SYNCDELAY;                    
        FIFORESET = tmp;
        break;

     case VR_NAKALL_OFF:
        tmp = FIFORESET;
        tmp &= ~bmNAKALL;      
        SYNCDELAY;                    
        FIFORESET = tmp;
        break;

  case USBFX2LK_SET_BARGRAPH_DISPLAY:
		BG_Set();
    break;

	case USBFX2LK_READ_BARGRAPH_DISPLAY:
		BG_Get();
		break;

	case USBFX2LK_READ_DEVINFO_LEN:
		EP0BUF[0] = LEN_DEVINFO;
		EP0BCH=0;
		SYNCDELAY;
		EP0BCL=1;
		SYNCDELAY;
		break;

	case USBFX2LK_READ_DEVINFO_DATA:
		lenTotal = LEN_DEVINFO;
		addr = devInfo;

		while ( lenTotal ) // Move requested data through EP0IN 
		{							     // one packet at a time.
			while(EP0CS & bmEPBUSY);
			if ( lenTotal < EP0BUFF_SIZE ) {
				lenSent = lenTotal;
			}
			else {
				lenSent = EP0BUFF_SIZE;
			}
			for ( i = 0; i < lenSent; i++ ) {
				*(EP0BUF+i) = *((BYTE xdata *)addr+i);
			}

			EP0BCH = 0;
			SYNCDELAY;
			EP0BCL = (BYTE)lenSent; // Arm endpoint with # bytes to transfer
			SYNCDELAY;

			addr += lenSent;
			lenTotal -= lenSent;
		}

		break;

     default:
        return(TRUE);
  }

  return(FALSE);
}

//-----------------------------------------------------------------------------
// USB Interrupt Handlers
//   The following functions are called by the USB interrupt jump table.
//-----------------------------------------------------------------------------

// Setup Data Available Interrupt Handler
void ISR_Sudav(void) interrupt 0
{
   GotSUD = TRUE;            // Set flag
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSUDAV;         // Clear SUDAV IRQ
}

// Setup Token Interrupt Handler
void ISR_Sutok(void) interrupt 0
{
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSUTOK;         // Clear SUTOK IRQ
}

void ISR_Sof(void) interrupt 0
{
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSOF;            // Clear SOF IRQ
}

void ISR_Ures(void) interrupt 0
{
   printf ( "USB Reset ISR triggered\r\n" );
   // whenever we get a USB reset, we should revert to full speed mode
   pConfigDscr = pFullSpeedConfigDscr;
   ((CONFIGDSCR xdata *) pConfigDscr)->type = CONFIG_DSCR;
   pOtherConfigDscr = pHighSpeedConfigDscr;
   ((CONFIGDSCR xdata *) pOtherConfigDscr)->type = OTHERSPEED_DSCR;

   EZUSB_IRQ_CLEAR();
   USBIRQ = bmURES;         // Clear URES IRQ
}

void ISR_Susp(void) interrupt 0
{
	printf ( "USB Suspend ISR triggered\r\n" );

	// turn off all bars
	IOD = IOD | 0xF0; //  below 4Pin

	// Display "S", tell people i'm sleep now
	IOD=IOD&0xF0;
	IOD+=0x0E;
	IOA=(IOA&0xF)+0xE0;
	
	Sleep = TRUE;
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmSUSP;
}

void ISR_Highspeed(void) interrupt 0
{
   if (EZUSB_HIGHSPEED())
   {
      pConfigDscr = pHighSpeedConfigDscr;
      ((CONFIGDSCR xdata *) pConfigDscr)->type = CONFIG_DSCR;
      pOtherConfigDscr = pFullSpeedConfigDscr;
      ((CONFIGDSCR xdata *) pOtherConfigDscr)->type = OTHERSPEED_DSCR;
   }

   EZUSB_IRQ_CLEAR();
   USBIRQ = bmHSGRANT;
}
void ISR_Ep0ack(void) interrupt 0
{
}
void ISR_Stub(void) interrupt 0
{
}
void ISR_Ep0in(void) interrupt 0
{
}
void ISR_Ep0out(void) interrupt 0
{
}
void ISR_Ep1in(void) interrupt 0
{
}
void ISR_Ep1out(void) interrupt 0
{
}
void ISR_Ep2inout(void) interrupt 0
{
}
void ISR_Ep4inout(void) interrupt 0
{
}
void ISR_Ep6inout(void) interrupt 0
{
}
void ISR_Ep8inout(void) interrupt 0
{
}
void ISR_Ibn(void) interrupt 0
{
}
void ISR_Ep0pingnak(void) interrupt 0
{
}
void ISR_Ep1pingnak(void) interrupt 0
{
}
void ISR_Ep2pingnak(void) interrupt 0
{
}
void ISR_Ep4pingnak(void) interrupt 0
{
}
void ISR_Ep6pingnak(void) interrupt 0
{
}
void ISR_Ep8pingnak(void) interrupt 0
{
}
void ISR_Errorlimit(void) interrupt 0
{
}
void ISR_Ep2piderror(void) interrupt 0
{
}
void ISR_Ep4piderror(void) interrupt 0
{
}
void ISR_Ep6piderror(void) interrupt 0
{
}
void ISR_Ep8piderror(void) interrupt 0
{
}
void ISR_Ep2pflag(void) interrupt 0
{
}
void ISR_Ep4pflag(void) interrupt 0
{
}
void ISR_Ep6pflag(void) interrupt 0
{
}
void ISR_Ep8pflag(void) interrupt 0
{
}
void ISR_Ep2eflag(void) interrupt 0
{
}
void ISR_Ep4eflag(void) interrupt 0
{
}
void ISR_Ep6eflag(void) interrupt 0
{
}
void ISR_Ep8eflag(void) interrupt 0
{
}
void ISR_Ep2fflag(void) interrupt 0
{
}
void ISR_Ep4fflag(void) interrupt 0
{
}
void ISR_Ep6fflag(void) interrupt 0
{
}
void ISR_Ep8fflag(void) interrupt 0
{
}
void ISR_GpifComplete(void) interrupt 0
{
}
void ISR_GpifWaveform(void) interrupt 0
{
}
