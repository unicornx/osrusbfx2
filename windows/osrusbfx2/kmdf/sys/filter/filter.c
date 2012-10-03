/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    filter.c

Abstract:

    This module shows how to a write a generic filter driver. The driver demonstrates how 
    to support device I/O control requests through queues. All the I/O requests passed on to 
    the lower driver. This filter driver shows how to handle IRP postprocessing by forwarding 
    the requests with and without a completion routine. To forward with a completion routine
    set the define FORWARD_REQUEST_WITH_COMPLETION to 1. 

Environment:

    Kernel mode

Modification tracking:

    This filter driver is create to demo how to send our new iotrls to device w/o touching the existing func
    driver - sometimes we may have no control over the existing func driver. Actually if you do have control
    over the function driver, would suggest skipping a filter entirely.
    There are two ways to talk to a lower filter.
    1) use a control device. the kmdf sidebad filter example does this. if you have more than one device to 
    filter, naming can be difficult if you have one control device per stack filtered. otherwise, if you have 
    one control device for all pnp stacks filtered, you will have to add some kind of way to address each stack 
    uniquely in your IO
    2) use a raw PDO. the kbdfiltr example shows how to do this. you can be a pnp device, so you can use device 
    interfaces to find it (no naming problem) and you have one raw pdo per stack filtered. You can easily merge 
    the kbdfiltr and kmdf toaster filter examples - that's what this driver does: based on 
    WinDDK\7600.16385.1\src\general\toaster\kmdf\filter\generic plus WinDDK\7600.16385.1\src\input\kbfiltr
    create a lowerfilter to send new ioctrl to device and create raw PDO to sideband the func drv w/o touch the func drv
    
Author         Date       Tracking Number  Description of Change
--------------+----------+---------------+--------------------------------------
unicornx       July/2012  N/A             filter drv
                                          
--*/

#include "filter.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, FilterEvtDeviceAdd)
#pragma alloc_text (PAGE, FilterEvtDevicePrepareHardware)
#pragma alloc_text (PAGE, FilterEvtIoDeviceControlFromRawPdo)
#pragma alloc_text (PAGE, GetDevInfoLength)
#pragma alloc_text (PAGE, GetDevInfoData)
#endif

ULONG InstanceNo = 0;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;
    WDFDRIVER           hDriver;

    KdPrint(("OSRUSBFX2 Filter Driver - Driver Framework Edition.\n"));
    KdPrint(("Built %s %s\n", __DATE__, __TIME__));

    //
    // Initiialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by manually setting the EvtDriverUnload in the
    // config structure. In general xxx_CONFIG_INIT macros are provided to
    // initialize most commonly used members.
    //

    WDF_DRIVER_CONFIG_INIT(
        &config,
        FilterEvtDeviceAdd
    );

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            &hDriver);
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
    }
    
    return status;
}


NTSTATUS
FilterEvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Here you can query the device properties
    using WdfFdoInitWdmGetPhysicalDevice/IoGetDeviceProperty and based
    on that, decide to create a filter device object and attach to the
    function stack. If you are not interested in filtering this particular
    instance of the device, you can just return STATUS_SUCCESS without creating
    a framework device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PFILTER_EXTENSION       filterExt;
    NTSTATUS                status;
    WDFDEVICE               device;    
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;
    WDFQUEUE                hQueue;
    
    PAGED_CODE ();

    UNREFERENCED_PARAMETER(Driver);

    KdPrint(("--> FilterEvtDeviceAdd\n"));

    //
    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);

	//
    // Initialize the pnpPowerCallbacks structure.  Callback events for PNP
    // and Power are specified here.  If you don't supply any callbacks,
    // the Framework will take appropriate default actions based on whether
    // DeviceInit is initialized to be an FDO, a PDO or a filter device
    // object.
    //

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    //
    // For usb devices, PrepareHardware callback is the to place select the
    // interface and configure the device.
    //
    pnpPowerCallbacks.EvtDevicePrepareHardware = FilterEvtDevicePrepareHardware;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);
	
    //
    // Specify the size of device extension where we track per device
    // context.
    //

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, FILTER_EXTENSION);
    
    //
    // Create a framework device object.This call will inturn create
    // a WDM deviceobject, attach to the lower stack and set the
    // appropriate flags and attributes.
    //
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }

    filterExt = FilterGetData(device);

    //
    // Configure the default queue to be Parallel. 
    // to handle IOCTLs that will be forwarded to us from
    // the rawPDO. 
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoDeviceControl = FilterEvtIoDeviceControlFromRawPdo;

    status = WdfIoQueueCreate(device,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &hQueue // pointer to default queue
                            );
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }   

	filterExt->rawPdoQueue = hQueue;

    //
    // Create a RAW pdo so we can provide a sideband communication with
    // the application. Please note that not filter drivers desire to
    // produce such a communication and not all of them are contrained
    // by other filter above which prevent communication thru the device
    // interface exposed by the main stack. So use this only if absolutely
    // needed. Also look at the toaster filter driver sample for an alternate
    // approach to providing sideband communication.
    //
    status = FiltrCreateRawPdo(device, ++InstanceNo);

    KdPrint(("<-- FilterEvtDeviceAdd\n"));
	
    return status;
}

NTSTATUS
FilterEvtDevicePrepareHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourceList,
    WDFCMRESLIST ResourceListTranslated
    )
/*++

Routine Description:

    In this callback, the driver does whatever is necessary to make the
    hardware ready to use.  In the case of a USB device, this involves
    reading and selecting descriptors.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    NTSTATUS                            status = STATUS_SUCCESS;
    PFILTER_EXTENSION                   pFilterContext = NULL;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    KdPrint(("--> FilterEvtDevicePrepareHardware\n"));

    pFilterContext = FilterGetData(Device);

    //
    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create device the first
    // the PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    // To make WdfUsbTargetDeviceCreate success, have to implement this filter as
    // a lowerfilter otherwise this call may fail due to the func driver - osrusbfx2 does 
    // not pass internal IOCTLs (which is how it configure and query USB properties via
    // URBs) down the stack.
    if (pFilterContext->UsbDevice == NULL) {
        status = WdfUsbTargetDeviceCreate(Device,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &pFilterContext->UsbDevice);
        if (!NT_SUCCESS(status)) {
            KdPrint(("WdfUsbTargetDeviceCreate failed with Status code %x\n", status));
            return status;
        }
		else {
			KdPrint(("WdfUsbTargetDeviceCreate success\n", status));
		}
    }    

    KdPrint(("<-- FilterEvtDevicePrepareHardware\n"));	

    return status;
}

VOID
FilterEvtIoDeviceControlFromRawPdo(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    
Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
    PFILTER_EXTENSION               filterExt;
    NTSTATUS                        status = STATUS_SUCCESS;
	size_t                          bytesReturned = 0;
    WDFDEVICE                       device;
	BYTE*                           pDevInfoLen; // len retrieved from device
	BYTE*                           pDevInfoLenInput; // len input from app
	BYTE*                           pDevInfoData; // data retrieved from device

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    KdPrint(("--> FilterEvtIoDeviceControlFromRawPdo\n"));

    device = WdfIoQueueGetDevice(Queue);

    filterExt = FilterGetData(device);

    switch (IoControlCode) {

    //
    // Put your cases for handling IOCTLs here
    //
    case IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN:
	    KdPrint(("Entered IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN\n"));

		//
        // Make sure the caller's output buffer is large enough
        // to hold the state of the bar graph
        //
        status = WdfRequestRetrieveOutputBuffer(Request,
                            sizeof(BYTE),
                            &pDevInfoLen,
                            NULL);

        if (!NT_SUCCESS(status)) {
			KdPrint(("User's output buffer is too small for this IOCTL, expecting an BYTE\n"));
            break;
        }
        //
        // Call our function to get
        //
        status = GetDevInfoLength(filterExt, pDevInfoLen);

		//
        // If we succeeded return the user their data
        //
        if (NT_SUCCESS(status)) {
            bytesReturned = sizeof(BYTE);

        } else {
            bytesReturned = 0;

        }
		
		KdPrint(("Completed IOCTL_OSRUSBFX2_FILTER_GETDEVINFOLEN\n"));		
        break;

    case IOCTL_OSRUSBFX2_FILTER_GETDEVINFODATA:
	    KdPrint(("Entered IOCTL_OSRUSBFX2_FILTER_GETDEVINFODATA\n"));

        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(BYTE),
                            &pDevInfoLenInput,
                            NULL);
        if (!NT_SUCCESS(status)) {
            KdPrint(("User's input buffer is too small for this IOCTL, expecting a BYTE\n"));
			bytesReturned = 0;
            break;
        }
		KdPrint(("Input length is %d\n", *pDevInfoLenInput));
		//
        // Make sure the caller's output buffer is large enough
        // to hold the state of the bar graph
        //
        status = WdfRequestRetrieveOutputBuffer(Request,
                            *pDevInfoLenInput,
                            &pDevInfoData,
                            NULL);

        if (!NT_SUCCESS(status)) {
			KdPrint(("User's output buffer is too small for this IOCTL, expecting an BYTE\n"));
			bytesReturned = 0;
            break;
        }
        //
        // Call our function to get
        //
        status = GetDevInfoData(filterExt, *pDevInfoLenInput, pDevInfoData, &bytesReturned );

		//
        // If we succeeded return the user their data
        //
        if (!NT_SUCCESS(status)) {
            bytesReturned = 0;
        }
		
		KdPrint(("Completed IOCTL_OSRUSBFX2_FILTER_GETDEVINFODATA\n"));		
        break;
		
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
		
    }
    
    WdfRequestCompleteWithInformation ( Request, status, bytesReturned );

    KdPrint(("<-- FilterEvtIoDeviceControlFromRawPdo\n"));
    
    return;
}

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS 
GetDevInfoLength(
    __in  PFILTER_EXTENSION filterExt, 
    __out BYTE*             pDevInfoLen
    )
/*++

Routine Description

    This routine gets the length of device info, so caller app
    can prepare proper buffer to retrieve the actual content info

Arguments:

    pDevInfoLen - One of our device extensions

Return Value:

    NT status value

--*/
{
    NTSTATUS status;

    WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS        sendOptions;
    WDF_MEMORY_DESCRIPTOR           memDesc;
    ULONG    bytesTransferred;

    PAGED_CODE();

    WDF_REQUEST_SEND_OPTIONS_INIT(
                                  &sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT
                                  );

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
                                         &sendOptions,
                                         WDF_REL_TIMEOUT_IN_SEC(5)
                                         );

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                        BmRequestDeviceToHost,
                                        BmRequestToDevice,
                                        USBFX2LK_READ_DEVINFO_LEN, // Request
                                        0, // Value
                                        0); // Index

    //
    // Set the buffer to 0, the board will OR in everything that is set
    //
    *pDevInfoLen = 0;


    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
                                      pDevInfoLen,
                                      sizeof(BYTE));

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
                                        filterExt->UsbDevice,
                                        WDF_NO_HANDLE, // Optional WDFREQUEST
                                        &sendOptions,
                                        &controlSetupPacket,
                                        &memDesc,
                                        &bytesTransferred);

    if(!NT_SUCCESS(status)) {

		KdPrint( ("GetDevInfoLength: Failed -  0x%x \n", status));

    } else {

		KdPrint( ("GetDevInfoLength: %d \n", *pDevInfoLen ));
    }

    return status;
}

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS 
GetDevInfoData(
    __in  PFILTER_EXTENSION filterExt,
    __in  ULONG             DevInfoLen,
    __out BYTE*             pDevInfoData,
    __out size_t*           bytesRead
    )
/*++

Routine Description

    This routine gets the data of the device

Arguments:

    pDevInfoLen - One of our device extensions

Return Value:

    NT status value

--*/
{
    NTSTATUS status;

    WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS        sendOptions;
    WDF_MEMORY_DESCRIPTOR           memDesc;
	ULONG                           bytesTransferred;

    PAGED_CODE();

    WDF_REQUEST_SEND_OPTIONS_INIT(
                                  &sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT
                                  );

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
                                         &sendOptions,
                                         WDF_REL_TIMEOUT_IN_SEC(5)
                                         );

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                        BmRequestDeviceToHost,
                                        BmRequestToDevice,
                                        USBFX2LK_READ_DEVINFO_DATA, // Request
                                        0, // Value
                                        0); // Index


    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
                                      pDevInfoData,
                                      DevInfoLen);

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
                                        filterExt->UsbDevice,
                                        WDF_NO_HANDLE, // Optional WDFREQUEST
                                        &sendOptions,
                                        &controlSetupPacket,
                                        &memDesc,
                                        &bytesTransferred);

    if(!NT_SUCCESS(status)) {

		KdPrint( ("GetDevInfoData: Failed -  0x%x \n", status));
		*bytesRead = 0;

    } else {

		KdPrint( ("GetDevInfoData: transferred bytes %d \n", bytesTransferred ));
		*bytesRead = (size_t)bytesTransferred;		
    }

    return status;
}


