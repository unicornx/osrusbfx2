
#ifndef _INCLUDED_FX2LPSERIAL_H
#define _INCLUDED_FX2LPSERIAL_H

#define FX2LP_SERIAL
#ifdef FX2LP_SERIAL


extern void FX2LPSerial_Init() ;

extern void
FX2LPSerial_XmitChar(char ch) reentrant;

extern void
FX2LPSerial_XmitHex1(BYTE b) ;

extern void
FX2LPSerial_XmitHex2(BYTE b) ;

extern void
FX2LPSerial_XmitHex4(WORD w) ;

extern void
FX2LPSerial_XmitString(char *str) reentrant;

/*---------------------------------------------------------------------------/
 Function:       printf
 Description:    A simple impelmentation of "printf" to Print formatted output to Serial port
                 For this simplified version, only support following format types:
 				 %s: string
                 %c: single-byte char
                 %b: BYTE in HEX format, using 'abcdef'
                 %x: WORD in HEX format, using 'abcdef'
 Param In/Out:   [in] fmt -- Format control
                 [in] ... -- Optional arguments
 Returns:        int: Returns the number of char printed, or a negative value if any error occurs.
----------------------------------------------------------------------------*/
extern int printf(const char *fmt, ...) reentrant;

#endif // FX2LP_SERIAL

#endif // _INCLUDED_FX2LPSERIAL_H
