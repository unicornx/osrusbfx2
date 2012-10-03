/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    filter.h

Abstract:

    Contains structure definitions and function prototypes for a generic filter driver.	

Environment:

    Kernel mode

--*/
#pragma warning(disable:4200)  // nameless struct/union
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int
#include <ntddk.h>
#include "usbdi.h"
#include <public.h>
#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)


#include <wdf.h>
#include <wdfusb.h>
#include <wdmsec.h> // for SDDLs
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#if !defined(_FILTER_H_)
#define _FILTER_H_


#define USBFX2LK_READ_DEVINFO_LEN           0xDC
#define USBFX2LK_READ_DEVINFO_DATA          0xDD

#define DRIVERNAME "filter.sys"

//
// Change the following define to 1 if you want to forward
// the request with a completion routine.
//
#define FORWARD_REQUEST_WITH_COMPLETION 0


typedef struct _FILTER_EXTENSION
{
	WDFUSBDEVICE UsbDevice;
    // More context data here

    //
    // Queue for handling requests that come from the rawPdo
    //
    WDFQUEUE rawPdoQueue;


}FILTER_EXTENSION, *PFILTER_EXTENSION;


WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILTER_EXTENSION,
                                   FilterGetData)

DRIVER_INITIALIZE                  DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD          FilterEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE    FilterEvtDevicePrepareHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FilterEvtIoDeviceControlFromRawPdo;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FilterEvtIoDeviceControlForRawPdo;


NTSTATUS 
GetDevInfoLength(
    __in  PFILTER_EXTENSION filterExt, 
    __out BYTE*             pDevInfoLen
    );

NTSTATUS 
GetDevInfoData(
    __in  PFILTER_EXTENSION filterExt, 
    __in  ULONG             DevInfoLen,    
    __out BYTE*             pDevInfoLen,
    __out size_t*           bytesTransferred
    );

//
// Used to identify filter bus. This guid is used as the enumeration string
// for the device id.
// {B6286080-ACDB-4e51-AA19-A88525E11E71}
DEFINE_GUID(GUID_BUS_OSRUSBFX2FILTER, 
0xb6286080, 0xacdb, 0x4e51, 0xaa, 0x19, 0xa8, 0x85, 0x25, 0xe1, 0x1e, 0x71);

#define  OSRUSBFX2FILTR_DEVICE_ID L"{B6286080-ACDB-4e51-AA19-A88525E11E71}\\OSRUSBFX2Filter\0"

typedef struct _RPDO_DEVICE_DATA
{

    ULONG InstanceNo;

    //
    // Queue of the parent device we will forward requests to
    //
    WDFQUEUE ParentQueue;

} RPDO_DEVICE_DATA, *PRPDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RPDO_DEVICE_DATA, PdoGetData)

NTSTATUS
FiltrCreateRawPdo(
    WDFDEVICE       Device,
    ULONG           InstanceNo
);


#endif // _FILTER_H_

