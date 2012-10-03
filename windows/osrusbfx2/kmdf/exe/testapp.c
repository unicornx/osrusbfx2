/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    TESTAPP.C

Abstract:

    Console test app for osrusbfx2 driver.

Environment:

    user mode only

Modification tracking:
Author         Date       Tracking Number  Description of Change
--------------+----------+---------------+--------------------------------------
unicornx       June/2012  N/A             modified to work with CY001 dev board
unicornx       July/2012  N/A             added one filter drv
--*/

            
#include <DriverSpecs.h>
__user_code 
 
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <conio.h>

#include "devioctl.h"
#include "strsafe.h"

#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <setupapi.h>
#include <basetyps.h>
#include "usbdi.h"
#include "public.h"

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)

#define WHILE(a) \
while(__pragma(warning(disable:4127)) a __pragma(warning(disable:4127)))

#define MAX_DEVPATH_LENGTH 256
#define NUM_ASYNCH_IO   100
#define BUFFER_SIZE     1024
#define READER_TYPE   1
#define WRITER_TYPE   2

BOOL G_fDumpUsbConfig = FALSE;    // flags set in response to console command line switches
BOOL G_fDumpReadData = FALSE;
BOOL G_fRead = FALSE;
BOOL G_fWrite = FALSE;
BOOL G_fPlayWithDevice = FALSE;
BOOL G_fPerformAsyncIo = FALSE;
ULONG G_IterationCount = 1; //count of iterations of the test we are to perform
int G_WriteLen = 512;         // #bytes to write
int G_ReadLen = 512;          // #bytes to read

BOOL
DumpUsbConfig( // defined in dump.c
    );

LONG  G_PosX;
LONG  G_PosY;

BOOL  G_MouseThreadRun; // TRUE: run, else stop
HANDLE G_hEvtMouseThreadStop = NULL; // signal to stop the running thread
DWORD WINAPI ReadMousePosistion ( LPVOID lpParam );
	
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

#if defined(BUILT_IN_DDK)

#define scanf_s scanf

#endif

BOOL
GetDevicePath(
    IN  LPGUID InterfaceGuid,
    __out_ecount(BufLen) PCHAR DevicePath,
    __in size_t BufLen
    )
{
    HDEVINFO HardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData = NULL;
    ULONG Length, RequiredLength = 0;
    BOOL bResult;
    HRESULT     hr;

    HardwareDeviceInfo = SetupDiGetClassDevs(
                             InterfaceGuid,
                             NULL,
                             NULL,
                             (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    if (HardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed!\n");
        return FALSE;
    }

    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bResult = SetupDiEnumDeviceInterfaces(HardwareDeviceInfo,
                                              0,
                                              InterfaceGuid,
                                              0,
                                              &DeviceInterfaceData);

    if (bResult == FALSE) {

        LPVOID lpMsgBuf;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL,
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPSTR) &lpMsgBuf,
                          0,
                          NULL
                          )) {

            printf("SetupDiEnumDeviceInterfaces failed: %s", (LPSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }

        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        return FALSE;
    }

    SetupDiGetDeviceInterfaceDetail(
        HardwareDeviceInfo,
        &DeviceInterfaceData,
        NULL,
        0,
        &RequiredLength,
        NULL
        );

    DeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
                        LocalAlloc(LMEM_FIXED, RequiredLength);

    if (DeviceInterfaceDetailData == NULL) {
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        printf("Failed to allocate memory.\n");
        return FALSE;
    }

    DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    Length = RequiredLength;

    bResult = SetupDiGetDeviceInterfaceDetail(
                  HardwareDeviceInfo,
                  &DeviceInterfaceData,
                  DeviceInterfaceDetailData,
                  Length,
                  &RequiredLength,
                  NULL);

    if (bResult == FALSE) {

        LPVOID lpMsgBuf;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL,
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPSTR) &lpMsgBuf,
                          0,
                          NULL)) {

            printf("Error in SetupDiGetDeviceInterfaceDetail: %s\n", (LPSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }

        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        LocalFree(DeviceInterfaceDetailData);
        return FALSE;
    }

    hr = StringCchCopy(DevicePath,
                       BufLen,
                       DeviceInterfaceDetailData->DevicePath);
    if (FAILED(hr)) {
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        LocalFree(DeviceInterfaceDetailData);
        return FALSE;
    }

    SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
    LocalFree(DeviceInterfaceDetailData);

    return TRUE;

}


HANDLE
OpenDevice(
    __in BOOL Synchronous
    )

/*++
Routine Description:

    Called by main() to open an instance of our device after obtaining its name

Arguments:

    Synchronous - TRUE, if Device is to be opened for synchronous access.
                  FALSE, otherwise.

Return Value:

    Device handle on success else INVALID_HANDLE_VALUE

--*/

{
    HANDLE hDev;
    char completeDeviceName[MAX_DEVPATH_LENGTH];

    if ( !GetDevicePath(
            (LPGUID) &GUID_DEVINTERFACE_OSRUSBFX2,
            completeDeviceName,
            sizeof(completeDeviceName)) )
    {
            return  INVALID_HANDLE_VALUE;
    }

    printf("DeviceName = (%s)\n", completeDeviceName);

    if(Synchronous) {
        hDev = CreateFile(completeDeviceName,
                GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                NULL, // default security
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
    } else {

        hDev = CreateFile(completeDeviceName,
                GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                NULL, // default security
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                NULL);
    }

    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open the device, error - %d", GetLastError());
    } else {
        printf("Opened the device OSR FX2 successfully.\n");
    }

    return hDev;
}

HANDLE
OpenDevice2()

/*++
Routine Description:

    Called by main() to open an instance of our filter device after obtaining its name

Arguments:


Return Value:

    Device handle on success else INVALID_HANDLE_VALUE

--*/

{
    HANDLE hDev;
    char completeDeviceName[MAX_DEVPATH_LENGTH];

    if ( !GetDevicePath(
            (LPGUID) &GUID_DEVINTERFACE_OSRUSBFX2FILTER,
            completeDeviceName,
            sizeof(completeDeviceName)) )
    {
            return  INVALID_HANDLE_VALUE;
    }

    printf("DeviceName = (%s)\n", completeDeviceName);

    hDev = CreateFile ( completeDeviceName,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL);

    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open the device, error - %d", GetLastError());
    } else {
        printf("Opened the device Filter for OSR FX2 successfully.\n");
    }

    return hDev;
}


VOID
Usage()

/*++
Routine Description:

    Called by main() to dump usage info to the console when
    the app is called with no parms or with an invalid parm

Arguments:

    None

Return Value:

    None

--*/

{
    printf("Usage for osrusbfx2 testapp:\n");
    printf("-r [n] where n is number of bytes to read\n");
    printf("-w [n] where n is number of bytes to write\n");
    printf("-c [n] where n is number of iterations (default = 1)\n");
    printf("-v verbose -- dumps read data\n");
    printf("-p to control bar LEDs, seven segment, and dip switch\n");
    printf("-a to perform asynchronous I/O\n");
    printf("-u to dump USB configuration and pipe info \n");

    return;
}


void
Parse(
    __in int argc,
    __in_ecount(argc) LPSTR  *argv
    )

/*++
Routine Description:

    Called by main() to parse command line parms

Arguments:

    argc and argv that was passed to main()

Return Value:

    Sets global flags as per user function request

--*/

{
    int i;

    if ( argc < 2 ) // give usage if invoked with no parms
        Usage();

    for (i=0; i<argc; i++) {
        if (argv[i][0] == '-' ||
            argv[i][0] == '/') {
            switch(argv[i][1]) {
            case 'r':
            case 'R':
                if (i+1 >= argc) {
                    Usage();
                    exit(1);
                }
                else {
                    G_ReadLen = atoi(&argv[i+1][0]);
                                    G_fRead = TRUE;
                }
                i++;
                break;
            case 'w':
            case 'W':
                if (i+1 >= argc) {
                    Usage();
                    exit(1);
                }
                else {
                    G_WriteLen = atoi(&argv[i+1][0]);
                                    G_fWrite = TRUE;
                }
                i++;
                break;
            case 'c':
            case 'C':
                if (i+1 >= argc) {
                    Usage();
                    exit(1);
                }
                else {
                    G_IterationCount = atoi(&argv[i+1][0]);
                }
                i++;
                break;
            case 'u':
            case 'U':
                G_fDumpUsbConfig = TRUE;
                break;
            case 'p':
            case 'P':
                G_fPlayWithDevice = TRUE;
                break;
            case 'a':
            case 'A':
                G_fPerformAsyncIo = TRUE;
                break;
            case 'v':
            case 'V':
                G_fDumpReadData = TRUE;
                break;
            default:
                Usage();
            }
        }
    }
}

BOOL
Compare_Buffs(
    __in char *buff1,
    __in char *buff2,
    __in int   length
    )
/*++
Routine Description:

    Called to verify read and write buffers match for loopback test

Arguments:

    buffers to compare and length

Return Value:

    TRUE if buffers match, else FALSE

--*/
{
    int ok = 1;

    if (memcmp(buff1, buff2, length )) {
        // Edi, and Esi point to the mismatching char and ecx indicates the
        // remaining length.
        ok = 0;
    }

    return ok;
}

#define NPERLN 8

VOID
Dump(
   UCHAR *b,
   int len
)

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
    PULONG pBuf = (PULONG) b;

    // dump an ordinal ULONG for each sizeof(ULONG)'th byte
    printf("\n****** BEGIN DUMP LEN decimal %d, 0x%x\n", len,len);
    for (i=0; i<longLen; i++) {
        printf("%04X ", *pBuf++);
        if (i % NPERLN == (NPERLN - 1)) {
            printf("\n");
        }
    }
    if (i % NPERLN != 0) {
        printf("\n");
    }
    printf("\n****** END DUMP LEN decimal %d, 0x%x\n", len,len);
}


BOOL
PlayWithDevice()
{
    HANDLE          deviceHandle = INVALID_HANDLE_VALUE;
    HANDLE          deviceHandle2 = INVALID_HANDLE_VALUE;
    DWORD           code;
    ULONG           index;
    INPUT_FUNCTION  function;
    BAR_GRAPH_STATE barGraphState;
    ULONG           bar;
    SWITCH_STATE    switchState;
    UCHAR           sevenSegment = 0;
    UCHAR           i;
	BYTE            devInfoLen;
	BYTE*           pdevInfoData;
    BOOL            result = FALSE;

    deviceHandle = OpenDevice(FALSE);
	
    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find any OSR FX2 devices!\n");

        return FALSE;

    }

	deviceHandle2 = OpenDevice2();
	
    if (deviceHandle2 == INVALID_HANDLE_VALUE) {

        printf("Unable to find any OSR FX2 Filter devices!\n");

		goto Error;
    }

    //
    // Infinitely print out the list of choices, ask for input, process
    // the request
    //
    WHILE(TRUE)  {

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
		printf ("\t12. Get device info\n");
        printf ("\n\t0. Exit\n");
        printf ("\n\tSelection: ");

        if (scanf_s ("%d", &function) <= 0) {

            printf("Error reading input!\n");
            goto Error;

        }

        switch(function)  {

        case LIGHT_ONE_BAR:

        printf("Which Bar (input number 1 thru %d)?\n", BARGRAPH_MAXBAR);
        if (scanf_s ("%d", &bar) <= 0) {

            printf("Error reading input!\n");
            goto Error;

        }

        if(bar == 0 || bar > BARGRAPH_MAXBAR){
            printf("Invalid bar number!\n");
            goto Error;
        }

        bar = bar - 1  + ( 8 - BARGRAPH_MAXBAR )  ; // normalize to correct offset

		barGraphState.BarsAsUChar = BARGRAPH_FLAGLIGHT;
		barGraphState.BarsAsUChar |=  ( 1 << (UCHAR)bar );

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                             &barGraphState,           // Ptr to InBuffer
                             sizeof(BAR_GRAPH_STATE),  // Length of InBuffer
                             NULL,         // Ptr to OutBuffer
                             0,            // Length of OutBuffer
                             &index,       // BytesReturned
                             0)) {         // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        break;

        case CLEAR_ONE_BAR:


        printf("Which Bar (input number 1 thru %d)?\n", BARGRAPH_MAXBAR);
        if (scanf_s ("%d", &bar) <= 0) {

            printf("Error reading input!\n");
            goto Error;

        }

        if(bar == 0 || bar > BARGRAPH_MAXBAR){
            printf("Invalid bar number!\n");
            goto Error;
        }

		bar = bar - 1  + ( 8 - BARGRAPH_MAXBAR )  ; // normalize to correct offset
		barGraphState.BarsAsUChar = BARGRAPH_FLAGCLEAR;
		barGraphState.BarsAsUChar |=  ( 1 << (UCHAR)bar );

        printf("Clearing it\n");
        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                             &barGraphState,         // Ptr to InBuffer
                             sizeof(BAR_GRAPH_STATE), // Length of InBuffer
                             NULL,             // Ptr to OutBuffer
                             0,            // Length of OutBuffer
                             &index,        // BytesReturned
                             0)) {          // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;

        }

        break;

        case LIGHT_ALL_BARS:

        barGraphState.BarsAsUChar = 0xF1;

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                             &barGraphState,         // Ptr to InBuffer
                             sizeof(BAR_GRAPH_STATE),   // Length of InBuffer
                             NULL,         // Ptr to OutBuffer
                             0,            // Length of OutBuffer
                             &index,       // BytesReturned
                             0)) {          // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        break;

        case CLEAR_ALL_BARS:

        barGraphState.BarsAsUChar = 0xF0;

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                             &barGraphState,                 // Ptr to InBuffer
                             sizeof(BAR_GRAPH_STATE),         // Length of InBuffer
                             NULL,             // Ptr to OutBuffer
                             0,            // Length of OutBuffer
                             &index,         // BytesReturned
                             0)) {        // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        break;


        case GET_BAR_GRAPH_LIGHT_STATE:

        barGraphState.BarsAsUChar = 0;

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY,
                             NULL,             // Ptr to InBuffer
                             0,            // Length of InBuffer
                             &barGraphState,          // Ptr to OutBuffer
                             sizeof(BAR_GRAPH_STATE), // Length of OutBuffer
                             &index,                  // BytesReturned
                             0)) {                   // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        printf("Bar Graph: \n");
        printf("    Bar8 is %s\n", barGraphState.Bar8 ? "ON" : "OFF");
        printf("    Bar7 is %s\n", barGraphState.Bar7 ? "ON" : "OFF");
        printf("    Bar6 is %s\n", barGraphState.Bar6 ? "ON" : "OFF");
        printf("    Bar5 is %s\n", barGraphState.Bar5 ? "ON" : "OFF");
        printf("    Bar4 is %s\n", barGraphState.Bar4 ? "ON" : "OFF");
        printf("    Bar3 is %s\n", barGraphState.Bar3 ? "ON" : "OFF");
        printf("    Bar2 is %s\n", barGraphState.Bar2 ? "ON" : "OFF");
        printf("    Bar1 is %s\n", barGraphState.Bar1 ? "ON" : "OFF");

        break;

        case GET_MOUSE_POSITION:

#if 0
        switchState.SwitchesAsUChar = 0;

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_READ_SWITCHES,
                             NULL,             // Ptr to InBuffer
                             0,            // Length of InBuffer
                             &switchState,          // Ptr to OutBuffer
                             sizeof(SWITCH_STATE),  // Length of OutBuffer
                             &index,                // BytesReturned
                             0)) {                  // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        printf("Switches: \n");
        printf("    Switch8 is %s\n", switchState.Switch8 ? "ON" : "OFF");
        printf("    Switch7 is %s\n", switchState.Switch7 ? "ON" : "OFF");
        printf("    Switch6 is %s\n", switchState.Switch6 ? "ON" : "OFF");
        printf("    Switch5 is %s\n", switchState.Switch5 ? "ON" : "OFF");
        printf("    Switch4 is %s\n", switchState.Switch4 ? "ON" : "OFF");
        printf("    Switch3 is %s\n", switchState.Switch3 ? "ON" : "OFF");
        printf("    Switch2 is %s\n", switchState.Switch2 ? "ON" : "OFF");
        printf("    Switch1 is %s\n", switchState.Switch1 ? "ON" : "OFF");
#endif
		break;

        case GET_MOUSE_POSITION_AS_INTERRUPT_MESSAGE:
{
		HANDLE          hThd = NULL;

		G_hEvtMouseThreadStop = CreateEvent ( 
			NULL,   // no SECURITY_ATTRIBUTES
			FALSE,  // auto-reset
			FALSE,  // initial state is nonsignaled.
			NULL ); // created without a name
		if ( NULL == G_hEvtMouseThreadStop ) {
			printf("CreateEvent for signal stop of mouse tracking thread failed with error 0x%x\n", 
				GetLastError());
			goto Error;
		}

        G_MouseThreadRun = TRUE;

		hThd = CreateThread(
			NULL,   // Default Security Attrib.
			0,      // Initial Stack Size,
			ReadMousePosistion,
			(LPVOID)deviceHandle,
			0,      // Runs immedieately after creation
			NULL ); // Don't need the Thread Id.

		if ( NULL == hThd ) {
			printf("Couldn't create mouse thread - error %d\n", GetLastError());
       		goto Error;
       	}

		printf("\r\nTracking mouse position: X=0, Y=0. Press any key to stop.");
		_getch();
		if ( !SetEvent ( G_hEvtMouseThreadStop ) ) 
        {
        	printf("SetEvent failed (%d)\n", GetLastError());
       		goto Error;
    	}
		G_MouseThreadRun = FALSE;
}
        break;

        case GET_7_SEGEMENT_STATE:

        sevenSegment = 0;

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY,
                             NULL,             // Ptr to InBuffer
                             0,            // Length of InBuffer
                             &sevenSegment,                 // Ptr to OutBuffer
                             sizeof(UCHAR),         // Length of OutBuffer
                             &index,                     // BytesReturned
                             0)) {                       // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        printf("7 Segment mask:  0x%x\n", sevenSegment);
        break;

        case SET_7_SEGEMENT_STATE:

        for (i = 0; i < 8; i++) {

            sevenSegment = 1 << i;

            if (!DeviceIoControl(deviceHandle,
                                 IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY,
                                 &sevenSegment,   // Ptr to InBuffer
                                 sizeof(UCHAR),  // Length of InBuffer
                                 NULL,           // Ptr to OutBuffer
                                 0,         // Length of OutBuffer
                                 &index,         // BytesReturned
                                 0)) {           // Ptr to Overlapped structure

                code = GetLastError();

                printf("DeviceIoControl failed with error 0x%x\n", code);

                goto Error;
            }

            printf("This is %d\n", i);
            Sleep(500);

        }

        printf("7 Segment mask:  0x%x\n", sevenSegment);
        break;

        case RESET_DEVICE:

        printf("Reset the device\n");

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_RESET_DEVICE,
                             NULL,             // Ptr to InBuffer
                             0,            // Length of InBuffer
                             NULL,                 // Ptr to OutBuffer
                             0,         // Length of OutBuffer
                             &index,   // BytesReturned
                             NULL)) {        // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        break;

		case GET_DEV_INFO:

		// ping FDO to wakeup the device in case it wents sleep
        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN,
                             NULL,             // Ptr to InBuffer
                             0,                // Length of InBuffer
                             &devInfoLen,      // Ptr to OutBuffer
                             sizeof(BYTE),     // Length of OutBuffer
                             &index,           // BytesReturned
                             NULL)) {             // Ptr to Overlapped structure

            printf("1) Ping device to wake it up.\n");
        }
		if (!DeviceIoControl(deviceHandle2,
                             IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN,
                             NULL,             // Ptr to InBuffer
                             0,                // Length of InBuffer
                             &devInfoLen,      // Ptr to OutBuffer
                             sizeof(BYTE),     // Length of OutBuffer
                             &index,           // BytesReturned
                             NULL)) {             // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }
		printf("2) Get device info length: %d\n", devInfoLen);

	    pdevInfoData = (BYTE*)LocalAlloc( LMEM_FIXED, devInfoLen+1 );
		if ( NULL == pdevInfoData ) {
			printf ( "LocalAlloc failed! exit...\n" );
			goto Error;
		}
		ZeroMemory( pdevInfoData, devInfoLen+1 );

		if (!DeviceIoControl(deviceHandle2,
                             IOCTL_OSRUSBFX2_FILTER_GETDEVINFODATA,
                             &devInfoLen,      // Ptr to InBuffer
                             sizeof (BYTE),    // Length of InBuffer
                             pdevInfoData,     // Ptr to OutBuffer
                             devInfoLen+1,     // Length of OutBuffer
                             &index,           // BytesReturned
                             NULL)) {          // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }
		if ( devInfoLen != index ) {
			printf ( "bytes returned length [%d] not matched! \n", index );
			goto Error;
		}
		printf( "3) Get device info data:\n" );
		printf( "------------------------------\n" );		
		printf( "%s", pdevInfoData );
		printf( "------------------------------\n" );		

		LocalFree(pdevInfoData);
		
		break;

        case REENUMERATE_DEVICE:

        printf("Re-enumerate the device\n");

        if (!DeviceIoControl(deviceHandle,
                             IOCTL_OSRUSBFX2_REENUMERATE_DEVICE,
                             NULL,             // Ptr to InBuffer
                             0,            // Length of InBuffer
                             NULL,                 // Ptr to OutBuffer
                             0,         // Length of OutBuffer
                             &index,   // BytesReturned
                             NULL)) {        // Ptr to Overlapped structure

            code = GetLastError();

            printf("DeviceIoControl failed with error 0x%x\n", code);

            goto Error;
        }

        //
        // Close the handle to the device and exit out so that
        // the driver can unload when the device is surprise-removed
        // and reenumerated.
        //
        default:

        result = TRUE;
        goto Error;

        }

    }   // end of while loop

Error:

    if ( INVALID_HANDLE_VALUE != deviceHandle ) CloseHandle(deviceHandle);

    if ( INVALID_HANDLE_VALUE != deviceHandle2 ) CloseHandle(deviceHandle2);
	
	if ( NULL != G_hEvtMouseThreadStop ) CloseHandle(G_hEvtMouseThreadStop);

	if ( NULL != pdevInfoData ) LocalFree(pdevInfoData);
	
    return result;

}


ULONG
AsyncIo(
    PVOID  ThreadParameter
    )
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    HANDLE hCompletionPort = NULL;
    OVERLAPPED *pOvList = NULL;
    PUCHAR      buf = NULL;
    ULONG_PTR    i;
    ULONG   ioType = (ULONG)(ULONG_PTR)ThreadParameter;
    ULONG   error;

    hDevice = OpenDevice(FALSE);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Cannot open device %d\n", GetLastError());
        goto Error;
    }

    hCompletionPort = CreateIoCompletionPort(hDevice, NULL, 1, 0);

    if (hCompletionPort == NULL) {
        printf("Cannot open completion port %d \n",GetLastError());
        goto Error;
    }

    pOvList = (OVERLAPPED *)malloc(NUM_ASYNCH_IO * sizeof(OVERLAPPED));

    if (pOvList == NULL) {
        printf("Cannot allocate overlapped array \n");
        goto Error;
    }

    buf = (PUCHAR)malloc(NUM_ASYNCH_IO * BUFFER_SIZE);

    if (buf == NULL) {
        printf("Cannot allocate buffer \n");
        goto Error;
    }

    ZeroMemory(pOvList, NUM_ASYNCH_IO * sizeof(OVERLAPPED));
    ZeroMemory(buf, NUM_ASYNCH_IO * BUFFER_SIZE);

    //
    // Issue asynch I/O
    //

    for (i = 0; i < NUM_ASYNCH_IO; i++) {
        if (ioType == READER_TYPE) {
            if ( ReadFile( hDevice,
                      buf + (i* BUFFER_SIZE),
                      BUFFER_SIZE,
                      NULL,
                      &pOvList[i]) == 0) {

                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf(" %d th read failed %d \n",i, GetLastError());
                    goto Error;
                }
            }

        } else {
            if ( WriteFile( hDevice,
                      buf + (i* BUFFER_SIZE),
                      BUFFER_SIZE,
                      NULL,
                      &pOvList[i]) == 0) {
                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf(" %d th write failed %d \n",i, GetLastError());
                    goto Error;
                }
            }
        }
    }

    //
    // Wait for the I/Os to complete. If one completes then reissue the I/O
    //

    WHILE (1) {
        OVERLAPPED *completedOv;
        ULONG_PTR   key;
        ULONG     numberOfBytesTransferred;

        if ( GetQueuedCompletionStatus(hCompletionPort, &numberOfBytesTransferred,
                            &key, &completedOv, INFINITE) == 0) {
            printf("GetQueuedCompletionStatus failed %d\n", GetLastError());
            goto Error;
        }

        //
        // Read successfully completed. Issue another one.
        //

        if (ioType == READER_TYPE) {

            i = completedOv - pOvList;

            printf("Number of bytes read by request number %d is %d\n",
                                i, numberOfBytesTransferred);

            if ( ReadFile( hDevice,
                      buf + (i * BUFFER_SIZE),
                      BUFFER_SIZE,
                      NULL,
                      completedOv) == 0) {
                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf("%d th Read failed %d \n", i, GetLastError());
                    goto Error;
                }
            }
        } else {

            i = completedOv - pOvList;

            printf("Number of bytes written by request number %d is %d\n",
                            i, numberOfBytesTransferred);

            if ( WriteFile( hDevice,
                      buf + (i * BUFFER_SIZE),
                      BUFFER_SIZE,
                      NULL,
                      completedOv) == 0) {
                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf("%d th write failed %d \n", i, GetLastError());
                    goto Error;
                }
            }
        }
    }

Error:
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
    }

    if (hCompletionPort) {
        CloseHandle(hCompletionPort);
    }

    if (pOvList) {
        free(pOvList);
    }
    if (buf) {
        free(buf);
    }

    return 1;

}


int
_cdecl
main(
    __in int argc,
    __in_ecount(argc) LPSTR  *argv
    )
/*++
Routine Description:

    Entry point to rwbulk.exe
    Parses cmdline, performs user-requested tests

Arguments:

    argc, argv  standard console  'c' app arguments

Return Value:

    Zero

--*/

{
    char * pinBuf = NULL;
    char * poutBuf = NULL;
    int    nBytesRead;
    int    nBytesWrite;
    int    ok;
    int    retValue = 0;
    UINT   success;
    HANDLE hRead = INVALID_HANDLE_VALUE;
    HANDLE hWrite = INVALID_HANDLE_VALUE;
    ULONG  fail = 0L;
    ULONG  i;

    Parse(argc, argv );

    //
    // dump USB configuation and pipe info
    //
    if (G_fDumpUsbConfig) {
        DumpUsbConfig();
    }

    if (G_fPlayWithDevice) {
        PlayWithDevice();
        goto exit;
    }

    if (G_fPerformAsyncIo) {
        HANDLE  th1;

        //
        // Create a reader thread
        //
        th1 = CreateThread( NULL,          // Default Security Attrib.
                            0,             // Initial Stack Size,
                            AsyncIo,       // Thread Func
                            (LPVOID)READER_TYPE,
                            0,             // Creation Flags
                            NULL );        // Don't need the Thread Id.

        if (th1 == NULL) {
            printf("Couldn't create reader thread - error %d\n", GetLastError());
            retValue = 1;
            goto exit;
        }

        //
        // Use this thread for peforming write.
        //
        AsyncIo((PVOID)WRITER_TYPE);

        goto exit;
    }

    //
    // doing a read, write, or both test
    //
    if ((G_fRead) || (G_fWrite)) {

        if (G_fRead) {
            if ( G_fDumpReadData ) { // round size to sizeof ULONG for readable dumping
                while( G_ReadLen % sizeof( ULONG ) ) {
                    G_ReadLen++;
                }
            }

            //
            // open the output file
            //
            hRead = OpenDevice(TRUE);
            if(hRead == INVALID_HANDLE_VALUE) {
                retValue = 1;
                goto exit;
            }

            pinBuf = malloc(G_ReadLen);
        }

        if (G_fWrite) {
            if ( G_fDumpReadData ) { // round size to sizeof ULONG for readable dumping
                while( G_WriteLen % sizeof( ULONG ) ) {
                    G_WriteLen++;
                }
            }

            //
            // open the output file
            //
            hWrite = OpenDevice(TRUE);
            if(hWrite == INVALID_HANDLE_VALUE) {
               retValue = 1;
               goto exit;
            }

            poutBuf = malloc(G_WriteLen);
        }

        for (i = 0; i < G_IterationCount; i++) {
            ULONG  j;

            if (G_fWrite && poutBuf && hWrite != INVALID_HANDLE_VALUE) {

                PULONG pOut = (PULONG) poutBuf;
                ULONG  numLongs = G_WriteLen / sizeof( ULONG );

                //
                // put some data in the output buffer
                //
                for (j=0; j<numLongs; j++) {
                    *(pOut+j) = j;
                }

                //
                // send the write
                //
                success = WriteFile(hWrite, poutBuf, G_WriteLen,  (PULONG) &nBytesWrite, NULL);
                if(success == 0) {
                    printf("WriteFile failed - error %d\n", GetLastError());
                    retValue = 1;
                    goto exit;
                }
                printf("Write (%04.4d) : request %06.6d bytes -- %06.6d bytes written\n",
                        i, G_WriteLen, nBytesWrite);

                assert(nBytesWrite == G_WriteLen);
            }

            if (G_fRead && pinBuf) {

                success = ReadFile(hRead, pinBuf, G_ReadLen, (PULONG) &nBytesRead, NULL);
                if(success == 0) {
                    printf("ReadFile failed - error %d\n", GetLastError());
                    retValue = 1;
                    goto exit;
                }

                printf("Read (%04.4d) : request %06.6d bytes -- %06.6d bytes read\n",
                       i, G_ReadLen, nBytesRead);

                if (G_fWrite) {

                    //
                    // validate the input buffer against what
                    // we sent to the 82930 (loopback test)
                    //
                    ok = Compare_Buffs(pinBuf, poutBuf,  nBytesRead);

                    if( G_fDumpReadData ) {
                        printf("Dumping read buffer\n");
                        Dump( (PUCHAR) pinBuf,  nBytesRead );
                        printf("Dumping write buffer\n");
                        Dump( (PUCHAR) poutBuf, nBytesRead );
                    }
                    assert(ok);

                    if(ok != 1) {
                        fail++;
                    }

                    assert(G_ReadLen == G_WriteLen);
                    assert(nBytesRead == G_ReadLen);
                }
            }
        }

    }

exit:

    if (pinBuf) {
        free(pinBuf);
    }

    if (poutBuf) {
        free(poutBuf);
    }

    // close devices if needed
    if (hRead != INVALID_HANDLE_VALUE) {
        CloseHandle(hRead);
    }

    if (hWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(hWrite);
    }

    return retValue;
}

DWORD WINAPI ReadMousePosistion ( LPVOID lpParam )
{
    HANDLE  deviceHandle;
	HANDLE	hEvtWaitDevIO = NULL;
	HANDLE	hEvents[2] = {NULL};
	OVERLAPPED overlap;
	BYTE    bKeyStatus;
	ULONG   index;
	DWORD   dwResult;
	BOOL    ExitStatus = TRUE;
	DWORD   dwErrCode;

	deviceHandle = (HANDLE)lpParam;

	hEvtWaitDevIO = CreateEvent ( 
		NULL,   // no SECURITY_ATTRIBUTES
		FALSE,  // auto-reset
		FALSE,  // initial state is nonsignaled.
		NULL ); // created without a name
	if ( NULL == hEvtWaitDevIO ) {
		printf("CreateEvent for wait DevIO failed with error 0x%x\n", GetLastError());
		ExitStatus = FALSE;
		goto Error;
	}

	ZeroMemory(&overlap, sizeof(OVERLAPPED));
	overlap.hEvent  = hEvtWaitDevIO;

	hEvents[0] = G_hEvtMouseThreadStop;
	hEvents[1] = hEvtWaitDevIO;

	G_PosX = G_PosY = 0;
	while ( G_MouseThreadRun ) {

		if ( !DeviceIoControl (
			deviceHandle,
            IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE,
            NULL,                   // Ptr to InBuffer
            0,                      // Length of InBuffer
            &bKeyStatus,            // Ptr to OutBuffer
            sizeof(bKeyStatus),     // Length of OutBuffer
            &index,                 // BytesReturned
            &overlap ) ) {          // Ptr to Overlapped structure

			dwErrCode = GetLastError();
			if ( ERROR_IO_PENDING != dwErrCode ) {
	            printf("\r\nDeviceIoControl failed with error 0x%x\n", GetLastError());
				ExitStatus = FALSE;
        	    goto Error;
			} // ERROR_IO_PENDING can be accepted, it just means 
			  // "Overlapped I/O operation is in progress."
        }

		// Wait infinitely to avoid too may pending ioctrl requests queued in driver
		// Please note the board may enter sleep if no mouse action occurs, so we
		// have to press WAKEUP key first then press mouse move keys.
		// Originally we use timeout waiting polling method, periodly iotrl will wakeup 
		// the board, but after use this infinite wait, the board may enter sleep
		// if no input for a long time. We have to wakeup the board from 
		// sleep by trigger WAKEUP PIN on the board by manual.
		dwResult = WaitForMultipleObjects ( 
			2, hEvents, 
			FALSE, // returns when the state of any one of the objects is set to signaled
			INFINITE ); // return only when the specified objects are signaled
		
		if ( WAIT_OBJECT_0 == dwResult ) {
			G_MouseThreadRun = FALSE;
			
		} 
		else if ( ( WAIT_OBJECT_0 + 1 ) == dwResult ) {
			if ( bKeyStatus & 0x1 ) {
				G_PosY++;
				}
			if ( bKeyStatus & 0x2 ) {
				G_PosY--;
			}
			if ( bKeyStatus & 0x4 ) {
				G_PosX--;
			}
			if ( bKeyStatus & 0x8 ) {
				G_PosX++;
			}
			// interval between two times of iocontrol should be short as possible
			// else we may lost some button input actions. Now maybe the most time
			// exhusting operaiton between two times of DeviceIoControl is this
			// printf. So you really care the accuracy error, you can try to move
			// GUI printf to another thread.
			printf ( "\rTracking mouse position: X=%d, Y=%d. Press any key to stop.",
				G_PosX, G_PosY );
		}
		else {
			// no WAIT_TIMEOUT due to we wait INFINITE
			printf ( "\rWaitForMultipleObjects failed with error 0x%x, exit!\r\n",
			 	GetLastError() );
			ExitStatus = FALSE;
			goto Error;
		}
	}

Error:
	if ( NULL != hEvtWaitDevIO ) CloseHandle(hEvtWaitDevIO);

	if ( FALSE == ExitStatus ) {
		printf ("\r\nTracking is stopped with exception. Press any key to stop.\r\n");
	}
	return 0;
}

