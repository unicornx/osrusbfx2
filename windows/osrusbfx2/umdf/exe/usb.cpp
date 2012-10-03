/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:

    usb.cpp

Abstract:

    A simple asynch test for usb driver.

Environment:

    user mode only

--*/

#define INITGUID

#include <driverspecs.h>
__user_code;
#include <windows.h>
#include <strsafe.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "WUDFOsrUsbPublic.h"

#define NUM_ASYNCH_IO   100
#define BUFFER_SIZE     1024

#define READER_TYPE   1
#define WRITER_TYPE   2

BOOLEAN G_PerformAsyncIo;
PCHAR DevicePath;

ULONG
AsyncIo(
    PVOID   ThreadParameter
    );

BOOLEAN
PerformWriteReadTest(
    IN HANDLE hDevice,
    IN ULONG TestLength    
    );

PCHAR
GetDevicePath(
    IN  LPGUID InterfaceGuid
    );

VOID 
__cdecl 
main(
    __in int argc, 
    __in_ecount(argc) PSTR argv[]
    )
{
    HANDLE hDevice;
    HANDLE  th1;
    BOOLEAN result;
        

    if (argc >= 2)  {        

#pragma prefast(push)
#pragma prefast(suppress:__WARNING_READ_OVERRUN, "size of argv is clearly at least 2 elements yet prefast incorrectly warns that it might be shorter")
        if(!strncmp (argv[1], "-Async", 6) ) {
#pragma prefast(pop)

            G_PerformAsyncIo = TRUE;
        } else {
            printf("Usage:\n");         
            printf("%s             --- Send single write and read request synchronously\n", argv[0]); 
            printf("%s -Async  --- Send %d reads and writes asynchronously\n",argv[0], NUM_ASYNCH_IO); 
            printf("Exit the app anytime by pressing Ctrl-C\n"); 
            exit(1);
        }
    }

    DevicePath = GetDevicePath((LPGUID)&GUID_DEVINTERFACE_OSRUSBFX2);

    printf("DevicePath: %s\n", DevicePath);

    hDevice = CreateFile(DevicePath,
                         GENERIC_READ|GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         0,
                         NULL );


    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device. Error %d\n",GetLastError());
        exit(1);
    } 

    printf("Opened device successfully\n");
        
    if(G_PerformAsyncIo) {
        
        printf("Starting AsyncIo\n");        
        
        //
        // Create a reader thread
        //
        th1 = CreateThread( NULL,          // Default Security Attrib.
                            0,                           // Initial Stack Size,
                            AsyncIo,                 // Thread Func
                            (LPVOID)READER_TYPE,
                            0,                            // Creation Flags
                            NULL );                      // Don't need the Thread Id.

        if (th1 == NULL) {
            printf("Couldn't create reader thread - error %d\n", GetLastError());
            exit(1);
        }
        
        // 
        // Use this thread for peforming write.
        //      
        AsyncIo((PVOID)WRITER_TYPE);
        
    } else {

        //
        // Write a pattern buffer and read it back, then verify it
        //
        result = PerformWriteReadTest(hDevice, 64);
        if(!result) {
            exit(1);
        }
    }

    CloseHandle(hDevice);
    
}

PUCHAR
CreatePatternBuffer(
    IN ULONG Length
    )
{
    unsigned int i;
    PUCHAR p, pBuf;

    pBuf = (PUCHAR)malloc(Length);
    if( pBuf == NULL ) {
        printf("Could not allocate %d byte buffer\n",Length);
        return NULL;
    }

    p = pBuf;

    for(i=0; i < Length; i++ ) {
        *p = (UCHAR)i;
        p++;
    }

    return pBuf;
}

BOOLEAN
VerifyPatternBuffer(
    IN PUCHAR pBuffer,
    IN ULONG  Length
    )
{
    unsigned int i;
    PUCHAR p = pBuffer;

    for( i=0; i < Length; i++ ) {

        if( *p != (UCHAR)(i & 0xFF) ) {
            printf("Pattern changed. SB 0x%x, Is 0x%x\n",
                   (UCHAR)(i & 0xFF), *p);
            return FALSE;
        }
   
        p++;
    }

    return TRUE;
}

BOOLEAN
PerformWriteReadTest(
    IN HANDLE hDevice,
    IN ULONG TestLength    
    )
/*
*/
{
    ULONG  bytesReturned =0;
    PUCHAR WriteBuffer = NULL,
                   ReadBuffer = NULL;
    BOOLEAN result = TRUE;

    WriteBuffer = CreatePatternBuffer(TestLength);
    if( WriteBuffer == NULL ) {

        result = FALSE;
        goto Cleanup;
    }

    ReadBuffer = (PUCHAR)malloc(TestLength);
    if( ReadBuffer == NULL ) {
        
        printf("PerformWriteReadTest: Could not allocate %d "
               "bytes ReadBuffer\n",TestLength);

         result = FALSE;
         goto Cleanup;

    }

    //
    // Write the pattern to the device
    //
    bytesReturned = 0;

    if (!WriteFile ( hDevice,
            WriteBuffer,
            TestLength,
            &bytesReturned,
            NULL)) {

        printf ("PerformWriteReadTest: WriteFile failed: "
                "Error %d\n", GetLastError());

        result = FALSE;
        goto Cleanup;

    } else {

        if( bytesReturned != TestLength ) {
                
            printf("bytes written is not test length! Written %d, "
                   "SB %d\n",bytesReturned, TestLength);

            result = FALSE;
            goto Cleanup;
        }

        printf ("%d Pattern Bytes Written successfully\n",
                bytesReturned);
    }

    bytesReturned = 0;

    if ( !ReadFile (hDevice,
            ReadBuffer,
            TestLength,
            &bytesReturned,
            NULL)) {

        printf ("PerformWriteReadTest: ReadFile failed: "
                "Error %d\n", GetLastError());

        result = FALSE;
        goto Cleanup;
        
    } else {

        if( bytesReturned != TestLength ) {

            printf("bytes Read is not test length! Read %d, "
                   "SB %d\n",bytesReturned, TestLength);

             //
             // Note: Is this a Failure Case??
             //
            result = FALSE;
            goto Cleanup;
        }

        printf ("%d Pattern Bytes Read successfully\n",bytesReturned);
    }

#if 0
    //
    // Now compare
    //
    if( !VerifyPatternBuffer(ReadBuffer, TestLength) ) {

        printf("Verify failed\n");

        result = FALSE;
        goto Cleanup;
    }

    printf("Pattern Verified successfully\n");
#endif

Cleanup:

    //
    // Free WriteBuffer if non NULL.
    //
    if (WriteBuffer) {
        free (WriteBuffer);
    }

    //
    // Free ReadBuffer if non NULL
    //
    if (ReadBuffer) {
        free (ReadBuffer);
    }
        
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
    ULONG     numberOfBytesTransferred;
    OVERLAPPED *completedOv;
    ULONG_PTR    i;
    ULONG   ioType = (ULONG)(ULONG_PTR)ThreadParameter;
    ULONG_PTR   key;
    ULONG   error = S_OK;

    hDevice = CreateFile(DevicePath,
                     GENERIC_WRITE|GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_FLAG_OVERLAPPED,
                     NULL );
    if (hDevice == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        printf("Cannot open %s error %d\n", DevicePath, error);
        goto Error;
    }

    hCompletionPort = CreateIoCompletionPort(hDevice, NULL, 1, 0);
    if (hCompletionPort == NULL) {
        error = GetLastError();
        printf("Cannot open completion port %d \n",error);
        goto Error;
    }

    pOvList = (OVERLAPPED *)malloc(NUM_ASYNCH_IO * sizeof(OVERLAPPED));
    if (pOvList == NULL) {
        printf("Cannot allocate overlapped array \n");
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto Error;
    }

    buf = (PUCHAR)malloc(NUM_ASYNCH_IO * BUFFER_SIZE);
    if (buf == NULL) {
        printf("Cannot allocate buffer \n");
        error = ERROR_NOT_ENOUGH_MEMORY;
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
                    printf(" %dth Read failed %d \n",i, error);
                    error = 1;
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
                    printf(" %dth Write failed %d \n",i, error);
                    error = 1;
                    goto Error;
                }
            }
        }
    }

    //
    // Wait for the I/Os to complete. If one completes then reissue the I/O
    //

    for (;;) {

        if ( GetQueuedCompletionStatus(hCompletionPort, &numberOfBytesTransferred, &key, &completedOv, INFINITE) == 0) {
            error = GetLastError();
            printf("GetQueuedCompletionStatus failed %d\n", error);
            break;
        }

        //
        // Read successfully completed. Issue another one.
        //

        if (ioType == READER_TYPE) {

            i = completedOv - pOvList;

            printf("Number of bytes read by request number %d is %d\n", i, numberOfBytesTransferred);
            
            if ( ReadFile( hDevice, 
                      buf + (i * BUFFER_SIZE), 
                      BUFFER_SIZE, 
                      NULL, 
                      completedOv) == 0) {
                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf("%dth Read failed %d \n", i, error);
                    break;
                }
            }
        } else {

            i = completedOv - pOvList;

            printf("Number of bytes written by request number %d is %d\n", i, numberOfBytesTransferred);

            if ( WriteFile( hDevice, 
                      buf + (i * BUFFER_SIZE), 
                      BUFFER_SIZE, 
                      NULL, 
                      completedOv) == 0) {
                error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    printf("%dth write failed %d \n", i, error);
                    break;
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
    return error;
    
}

PCHAR
GetDevicePath(
    IN  LPGUID InterfaceGuid
    )
{
    HDEVINFO HardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData = NULL;
    ULONG Length, RequiredLength = 0;
    BOOL bResult;

    HardwareDeviceInfo = SetupDiGetClassDevs(
                             InterfaceGuid,
                             NULL,
                             NULL,
                             (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    if (HardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed!\n");
        exit(1);
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

            printf("Error: %s", (LPSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }

        printf("SetupDiEnumDeviceInterfaces failed.\n");
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        exit(1);
    }

    SetupDiGetDeviceInterfaceDetail(
        HardwareDeviceInfo,
        &DeviceInterfaceData,
        NULL,
        0,
        &RequiredLength,
        NULL
        );

    DeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LMEM_FIXED, RequiredLength);

    if (DeviceInterfaceDetailData == NULL) {
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);       
        printf("Failed to allocate memory.\n");
        exit(1);
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

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPSTR) &lpMsgBuf,
                      0,
                      NULL
                      );

        printf(
            "SetupDiGetDeviceInterfaceDetail failed. Error: %s\n", 
            (LPCTSTR) lpMsgBuf);
        
        LocalFree(lpMsgBuf);
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);       
        LocalFree(DeviceInterfaceDetailData);
        exit(1);
    }

    return DeviceInterfaceDetailData->DevicePath;
    
}


