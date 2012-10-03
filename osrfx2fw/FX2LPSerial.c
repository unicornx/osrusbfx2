
#include "fx2.h"
#include "fx2regs.h"
#include "FX2LPserial.h"

#include "stdarg.h"

void FX2LPSerial_Init()  // initializes the registers for using Timer2 as baud rate generator for a Baud rate of 38400.
{
	T2CON = 0x34 ;
	RCAP2H  = 0xFF ;
	RCAP2L = 0xD9;
	SCON0 = 0x5A ;
	TI = 1;

	CPUCS = ((CPUCS & ~bmCLKSPD) | bmCLKSPD1) ;	//Setting up the clock frequency

	FX2LPSerial_XmitString("\r\n->") ;		//Clearing the output screen
	 
}

void FX2LPSerial_XmitChar(char ch) reentrant // prints a character
{
	while (TI == 0) ;
	TI = 0 ;
	SBUF0 = ch ;	//print the character
}

void FX2LPSerial_XmitHex1(BYTE b) // intermediate function to print the 4-bit nibble in hex format
{
	if (b < 10)
		FX2LPSerial_XmitChar(b + '0') ;
	else
		FX2LPSerial_XmitChar(b - 10 + 'A') ;
}

void FX2LPSerial_XmitHex2(BYTE b) // prints the value of the BYTE variable in hex
{
	FX2LPSerial_XmitHex1((b >> 4) & 0x0f) ;
	FX2LPSerial_XmitHex1(b & 0x0f) ;
}

void FX2LPSerial_XmitHex4(WORD w) // prints the value of the WORD variable in hex
{
	FX2LPSerial_XmitHex2((w >> 8) & 0xff) ;
	FX2LPSerial_XmitHex2(w & 0xff) ;
}

void FX2LPSerial_XmitString(char *str) reentrant
{
	while (*str)
		FX2LPSerial_XmitChar(*str++) ;
}

int printf (const char *fmt, ...) reentrant
{
    const char *s;
	char c;
	BYTE b;
    WORD w;
	int ret = 0;
    
	va_list ap;
    va_start(ap, fmt);

    while ( *fmt ) {
        if ( *fmt != '%' ) {
            FX2LPSerial_XmitChar ( *fmt++ );
			ret++;
            continue;
        }
		else
		{
			fmt++;
			if ( 0x00 == *fmt ) {
				// for the case the format is end with '%'
				FX2LPSerial_XmitChar ( '%' );
				ret++;
				break;
			}
		}

        switch ( *fmt ) {
        case 's':
            s = va_arg ( ap, const char * );
            for ( ; *s; s++ ) {
                FX2LPSerial_XmitChar ( *s );
				ret++;
            }
            break;
		case 'c':
			c = va_arg ( ap, char );
			FX2LPSerial_XmitChar ( c ) ;
			ret++;
            break;
		case 'b':
			b = va_arg ( ap, BYTE );
			FX2LPSerial_XmitHex2 ( b );
			ret += 2;
            break;
        case 'x':
            w = va_arg ( ap, WORD );
			FX2LPSerial_XmitHex4 ( w );
			ret += 4;
			break;
            /* Add other specifiers here... */              
        default:
			// for unsupport type, just print out as it is
			FX2LPSerial_XmitChar ( '%' );
			FX2LPSerial_XmitChar ( *fmt );
			ret += 2;
            break;
        }
        fmt++;
    }
	
	va_end(ap);

    return ret;
}

// for self test purpose only
#if 0
void test1()
{
	FX2LPSerial_XmitString("Char:"); 
	FX2LPSerial_XmitChar(0x02);
	FX2LPSerial_XmitString("\r\n"); 

	FX2LPSerial_XmitString("Hex1:"); 
	FX2LPSerial_XmitHex1(0x12) ;
	FX2LPSerial_XmitString("\r\n"); 

	FX2LPSerial_XmitString("Hex2:"); 
	FX2LPSerial_XmitHex2(0x12) ;
	FX2LPSerial_XmitString("\r\n"); 

	FX2LPSerial_XmitString("Hex4:"); 
	FX2LPSerial_XmitHex4(0x12) ;
	FX2LPSerial_XmitString("\r\n"); 
}

void test2 () 
{
	FX2LPSerial_XmitString("example %s:\r\n");	
	ret = _printf ("%s", "this is a string\r\n");
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("example %c:\r\n");	
	ret = _printf ("%c", 'A');
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("example %x (byte):\r\n");	
	ret = _printf ("%b", 'A');
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");
 
	FX2LPSerial_XmitString("example %x (word):\r\n");	
	ret = _printf ("%x", 0x1234);
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("example composite:\r\n");	
	ret = _printf ("this is string:%s, char:%c, byte:%b, word:%x\r\n", "hello moto!", 'A', 0x41, 0x1234 );
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("exception1:\r\n");	
	ret = _printf ("%" );
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("exception2:\r\n");	
	ret = _printf ("%5" );
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");

	FX2LPSerial_XmitString("exception3:\r\n");	
	ret = _printf ("no format, but with arg\r\n", "ddd", 19 );
	FX2LPSerial_XmitString("\r\nreturn is: 0x");
	FX2LPSerial_XmitHex4(ret) ;
	FX2LPSerial_XmitString("\r\n");
}
#endif
