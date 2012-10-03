/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    Device.cpp

Abstract:

    This module contains the implementation of the UMDF OSR Fx2 driver's
    device callback object.

Environment:

   Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"
#include "initguid.h"
#include "usb_hw.h"

#include "device.tmh"
#define CONCURRENT_READS 2

CMyDevice::~CMyDevice(
    )
{
    SAFE_RELEASE(m_pIoTargetInterruptPipeStateMgmt); 
}

HRESULT
CMyDevice::CreateInstance(
    __in IWDFDriver *FxDriver,
    __in IWDFDeviceInitialize * FxDeviceInit,
    __out PCMyDevice *Device
    )
/*++
 
  Routine Description:

    This method creates and initializs an instance of the OSR Fx2 driver's 
    device callback object.

  Arguments:

    FxDeviceInit - the settings for the device.

    Device - a location to store the referenced pointer to the device object.

  Return Value:

    Status

--*/
{
    PCMyDevice device;
    HRESULT hr;

    //
    // Allocate a new instance of the device class.
    //

    device = new CMyDevice();

    if (NULL == device)
    {
        return E_OUTOFMEMORY;
    }

    //
    // Initialize the instance.
    //

    hr = device->Initialize(FxDriver, FxDeviceInit);

    if (SUCCEEDED(hr)) 
    {
        *Device = device;
    } 
    else 
    {
        device->Release();
    }

    return hr;
}

HRESULT
CMyDevice::Initialize(
    __in IWDFDriver           * FxDriver,
    __in IWDFDeviceInitialize * FxDeviceInit
    )
/*++
 
  Routine Description:

    This method initializes the device callback object and creates the
    partner device object.

    The method should perform any device-specific configuration that:
        *  could fail (these can't be done in the constructor)
        *  must be done before the partner object is created -or-
        *  can be done after the partner object is created and which aren't 
           influenced by any device-level parameters the parent (the driver
           in this case) might set.

  Arguments:

    FxDeviceInit - the settings for this device.

  Return Value:

    status.

--*/
{
    IWDFDevice *fxDevice = NULL;

    HRESULT hr = S_OK;

    
    //
    // TODO: If you're writing a filter driver then indicate that here. 
    //
    // FxDeviceInit->SetFilter();
    //

    //
    // Set no locking unless you need an automatic callbacks synchronization
    //

    FxDeviceInit->SetLockingConstraint(None);

    //
    // Only one driver in the stack can be the Power policy owner (PPO).
    //
    // NOTE: If we want UMDF to be the PPO we also ask WinUsb.sys 
    // to not set itself as the PPO by setting the
    // WinUsbPowerPolicyOwnershipDisabled key through 
    // an AddReg in the INF.
    //     
#if defined(_NOT_POWER_POLICY_OWNER_)
    FxDeviceInit->SetPowerPolicyOwnership(FALSE);        
#else
    FxDeviceInit->SetPowerPolicyOwnership(TRUE);        
#endif
    
    //
    // TODO: Any per-device initialization which must be done before 
    //       creating the partner object.
    //

    //
    // Create a new FX device object and assign the new callback object to 
    // handle any device level events that occur.
    //

    //
    // QueryIUnknown references the IUnknown interface that it returns
    // (which is the same as referencing the device).  We pass that to 
    // CreateDevice, which takes its own reference if everything works.
    //

    if (SUCCEEDED(hr)) 
    {
        IUnknown *unknown = this->QueryIUnknown();

        hr = FxDriver->CreateDevice(FxDeviceInit, unknown, &fxDevice);

        unknown->Release();
    }

    //
    // If that succeeded then set our FxDevice member variable.
    //

    if (SUCCEEDED(hr)) 
    {
        //
        // Drop the reference we got from CreateDevice.  Since this object
        // is partnered with the framework object they have the same 
        // lifespan - there is no need for an additional reference.
        //
        
        fxDevice->Release();

        IWDFDevice2 *fxDevice2 = NULL;
        
        HRESULT hrQI = fxDevice->QueryInterface(__uuidof(IWDFDevice2), (void**) &fxDevice2);  
        
        WUDF_TEST_DRIVER_ASSERT(SUCCEEDED(hrQI) && fxDevice2);

        m_FxDevice = fxDevice2;
        
        //
        // Drop the reference we got from QueryInterface(). Since this object
        // is partnered with the framework object they have the same 
        // lifespan - there is no need for an additional reference.
        //
        
        fxDevice2->Release();        
    }

    return hr;
}

HRESULT
CMyDevice::Configure(
    VOID
    )
/*++
 
  Routine Description:

    This method is called after the device callback object has been initialized 
    and returned to the driver.  It would setup the device's queues and their 
    corresponding callback objects.

  Arguments:

    FxDevice - the framework device object for which we're handling events.

  Return Value:

    status

--*/
{   
    HRESULT hr = S_OK;

    hr = CMyReadWriteQueue::CreateInstance(this, &m_ReadWriteQueue);

    if (FAILED(hr))
    {
        return hr;
    }

    //
    // We use default queue for read/write
    //
    
    hr = m_ReadWriteQueue->Configure();

    m_ReadWriteQueue->Release();

    //
    // Create the control queue and configure forwarding for IOCTL requests.
    //

    if (SUCCEEDED(hr)) 
    {
        hr = CMyControlQueue::CreateInstance(this, &m_ControlQueue);

        if (SUCCEEDED(hr)) 
        {
            hr = m_ControlQueue->Configure();
            if (SUCCEEDED(hr)) 
            {
                m_FxDevice->ConfigureRequestDispatching(
                                m_ControlQueue->GetFxQueue(),
                                WdfRequestDeviceIoControl,
                                true
                                );
            }
            m_ControlQueue->Release();
        }
    }

    //
    // Create a manual I/O queue to hold requests for notification when
    // the switch state changes.
    //
    // We provide the device as the callback object even though the 
    // manual queue won't raise any events.
    //

    IUnknown * pQueueCallbackInterface = this->QueryIUnknown();

    hr = m_FxDevice->CreateIoQueue(pQueueCallbackInterface,
                                   FALSE,
                                   WdfIoQueueDispatchManual,
                                   false,
                                   false,
                                   &m_SwitchChangeQueue);
    
    //
    // Fx queue would take its own reference, release the reference that QueryIUnknown took
    //
    
    SAFE_RELEASE(pQueueCallbackInterface);

    //
    // Release creation reference as object tree will keep a reference
    //
    
    m_SwitchChangeQueue->Release();

    if (SUCCEEDED(hr)) 
    {
        hr = m_FxDevice->CreateDeviceInterface(&GUID_DEVINTERFACE_OSRUSBFX2,
                                               NULL);
    }

    return hr;
}

HRESULT
CMyDevice::QueryInterface(
    __in REFIID InterfaceId,
    __deref_out PVOID *Object
    )
/*++
 
  Routine Description:

    This method is called to get a pointer to one of the object's callback
    interfaces.  

  Arguments:

    InterfaceId - the interface being requested

    Object - a location to store the interface pointer if successful

  Return Value:

    S_OK or E_NOINTERFACE

--*/
{
    HRESULT hr;

    if (IsEqualIID(InterfaceId, __uuidof(IPnpCallbackHardware)))
    {
        *Object = QueryIPnpCallbackHardware();
        hr = S_OK;    
    }
    else if (IsEqualIID(InterfaceId, __uuidof(IPnpCallback)))
    {
        *Object = QueryIPnpCallback();
        hr = S_OK;    
    }     
    else if (IsEqualIID(InterfaceId, __uuidof(IUsbTargetPipeContinuousReaderCallbackReadersFailed))) 
    {    
        *Object = QueryContinousReaderFailureCompletion();
        hr = S_OK;  
    } 
    else if( IsEqualIID(InterfaceId, __uuidof(IUsbTargetPipeContinuousReaderCallbackReadComplete))) 
    {    
        *Object = QueryContinousReaderCompletion();
        hr = S_OK;  
    }
    else
    {
        hr = CUnknown::QueryInterface(InterfaceId, Object);
    }

    return hr;
}

HRESULT
CMyDevice::OnPrepareHardware(
    __in IWDFDevice * /* FxDevice */
    )
/*++

Routine Description:

    This routine is invoked to ready the driver
    to talk to hardware. It opens the handle to the 
    device and talks to it using the WINUSB interface.
    It invokes WINUSB to discver the interfaces and stores
    the information related to bulk endpoints.

Arguments:

    FxDevice  : Pointer to the WDF device interface

Return Value:

    HRESULT 

--*/
{
    PWSTR deviceName = NULL;
    DWORD deviceNameCch = 0;

    HRESULT hr;

    //
    // Get the device name.
    // Get the length to allocate first
    //

    hr = m_FxDevice->RetrieveDeviceName(NULL, &deviceNameCch);

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Cannot get device name %!HRESULT!",
                    hr
                    );
    }

    //
    // Allocate the buffer
    //

    if (SUCCEEDED(hr))
    {
        deviceName = new WCHAR[deviceNameCch];

        if (deviceName == NULL)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    //
    // Get the actual name
    //

    if (SUCCEEDED(hr))
    {
        hr = m_FxDevice->RetrieveDeviceName(deviceName, &deviceNameCch);

        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Cannot get device name %!HRESULT!",
                        hr
                        );
        }
    }

    if (SUCCEEDED(hr))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Device name %S",
                    deviceName
                    );
    }

    //
    // Create USB I/O Targets and configure them
    //

    if (SUCCEEDED(hr))
    {
        hr = CreateUsbIoTargets();
    }

    if (SUCCEEDED(hr))
    {
        ULONG length = sizeof(m_Speed);

        hr = m_pIUsbTargetDevice->RetrieveDeviceInformation(DEVICE_SPEED, 
                                                            &length,
                                                            &m_Speed);
        if (FAILED(hr)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Cannot get usb device speed information %!HRESULT!",
                        hr
                        );
        }
    }

    if (SUCCEEDED(hr)) 
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Speed - %x\n",
                    m_Speed
                    );
    }

    if (SUCCEEDED(hr))
    {
        hr = ConfigureUsbPipes();
    }

    //
    // Setup power-management settings on the device.
    //

    if (SUCCEEDED(hr))
    {
        hr = SetPowerManagement();
    }

    //
    // Clear the seven segement display to indicate that we're done with 
    // prepare hardware.
    //

    if (SUCCEEDED(hr))
    {
        hr = IndicateDeviceReady();
    }

#if defined(_NOT_POWER_POLICY_OWNER_)
    //
    // We have non-power managed queues, so we Stop them in OnReleaseHardware
    // and start them in OnPrepareHardware
    //
    
    if (SUCCEEDED(hr))
    {
        m_ReadWriteQueue->Start();
        m_ControlQueue->Start();        
    }
#endif

    if (SUCCEEDED(hr))
    {
        hr = ConfigContReaderForInterruptEndPoint();
    }

    delete[] deviceName;

    return hr;
}

HRESULT
CMyDevice::OnReleaseHardware(
    __in IWDFDevice * /* FxDevice */
    )
/*++

Routine Description:

    This routine is invoked when the device is being removed or stopped
    It releases all resources allocated for this device.

Arguments:

    FxDevice - Pointer to the Device object.

Return Value:

    HRESULT - Always succeeds.

--*/
{
#if defined(_NOT_POWER_POLICY_OWNER_)
    // If we have non-power managed queues, we need to Stop them
    // explicitly.
    //
    // We need to stop them before deleting I/O targets otherwise we
    // will continue to get I/O and our I/O processing will try to access
    // freed I/O targets
    //
    // We initialize queues in CMyDevice::Initialize so we can't get
    // here with queues being NULL and don't need to guard against that
    //
    
    m_ReadWriteQueue->StopSynchronously();
    m_ControlQueue->StopSynchronously();
#endif

    //
    // Delete USB Target Device WDF Object, this will in turn
    // delete all the children - interface and the pipe objects
    //
    // This makes sure that 
    //    1. We drain the I/O before releasing the targets
    //        a. We always need to do that for the pending read which does 
    //           not come from an I/O queue
    //        b. We need to do this even for I/O coming from I/O queues if
    //           we set them to non-power managed queues (to leverage wait/wake
    //           from WinUsb.sys)
    //    2. We remove USB target objects from object tree (and thereby free them)
    //       before any potential subsequent OnPrepareHardware creates new ones
    //
    // m_pIUsbTargetDevice could be NULL if OnPrepareHardware failed so we need
    // to guard against that
    //

    if (m_pIUsbTargetDevice)
    {
        m_pIUsbTargetDevice->DeleteWdfObject();
    }
    
    return S_OK;
}

HRESULT
CMyDevice::CreateUsbIoTargets(
    )
/*++

Routine Description:

    This routine creates Usb device, interface and pipe objects

Arguments:

    None

Return Value:

    HRESULT
--*/
{
    HRESULT                 hr;
    UCHAR                   NumEndPoints = 0;
    IWDFUsbTargetFactory *  pIUsbTargetFactory = NULL;
    IWDFUsbTargetDevice *   pIUsbTargetDevice = NULL;
    IWDFUsbInterface *      pIUsbInterface = NULL;
    IWDFUsbTargetPipe *     pIUsbPipe = NULL;
    
    hr = m_FxDevice->QueryInterface(IID_PPV_ARGS(&pIUsbTargetFactory));

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Cannot get usb target factory %!HRESULT!",
                    hr
                    );        
    }

    if (SUCCEEDED(hr)) 
    {
        hr = pIUsbTargetFactory->CreateUsbTargetDevice(
                                                  &pIUsbTargetDevice);
        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Unable to create USB Device I/O Target %!HRESULT!",
                        hr
                        );        
        }
        else
        {
            m_pIUsbTargetDevice = pIUsbTargetDevice;

            //
            // Release the creation reference as object tree will maintain a reference
            //
            
            pIUsbTargetDevice->Release();            
        }
    }

    if (SUCCEEDED(hr)) 
    {
        UCHAR NumInterfaces = pIUsbTargetDevice->GetNumInterfaces();

        WUDF_TEST_DRIVER_ASSERT(1 == NumInterfaces);
        
        hr = pIUsbTargetDevice->RetrieveUsbInterface(0, &pIUsbInterface);
        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Unable to retrieve USB interface from USB Device I/O Target %!HRESULT!",
                        hr
                        );        
        }
        else
        {
            m_pIUsbInterface = pIUsbInterface;

            pIUsbInterface->Release(); //release creation reference                        
        }
    }

    if (SUCCEEDED(hr)) 
    {
        NumEndPoints = pIUsbInterface->GetNumEndPoints();

        if (NumEndPoints != NUM_OSRUSB_ENDPOINTS) {
            hr = E_UNEXPECTED;
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Has %d endpoints, expected %d, returning %!HRESULT! ", 
                        NumEndPoints,
                        NUM_OSRUSB_ENDPOINTS,
                        hr
                        );
        }
    }

    if (SUCCEEDED(hr)) 
    {
        for (UCHAR PipeIndex = 0; PipeIndex < NumEndPoints; PipeIndex++)
        {
            hr = pIUsbInterface->RetrieveUsbPipeObject(PipeIndex, 
                                                  &pIUsbPipe);

            if (FAILED(hr))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            TEST_TRACE_DEVICE, 
                            "%!FUNC! Unable to retrieve USB Pipe for PipeIndex %d, %!HRESULT!",
                            PipeIndex,
                            hr
                            );        
            }
            else
            {
                if ( pIUsbPipe->IsInEndPoint() )
                {
                    if ( UsbdPipeTypeInterrupt == pIUsbPipe->GetType() )
                    {
                        m_pIUsbInterruptPipe = pIUsbPipe;

                        WUDF_TEST_DRIVER_ASSERT(m_pIoTargetInterruptPipeStateMgmt == NULL);

                        hr = m_pIUsbInterruptPipe->QueryInterface(__uuidof(
                                                   IWDFIoTargetStateManagement),
                                                   reinterpret_cast<void**>(&m_pIoTargetInterruptPipeStateMgmt)
                                                   );
                        if (FAILED(hr))
                        {
                            m_pIoTargetInterruptPipeStateMgmt = NULL;
                        }                        
                    }
                    else if ( UsbdPipeTypeBulk == pIUsbPipe->GetType() )
                    {
                        m_pIUsbInputPipe = pIUsbPipe;
                    }
                    else
                    {
                        pIUsbPipe->DeleteWdfObject();
                    }                      
                }
                else if ( pIUsbPipe->IsOutEndPoint() && (UsbdPipeTypeBulk == pIUsbPipe->GetType()) )
                {
                    m_pIUsbOutputPipe = pIUsbPipe;
                }
                else
                {
                    pIUsbPipe->DeleteWdfObject();
                }
    
                SAFE_RELEASE(pIUsbPipe); //release creation reference
            }
        }

        if (NULL == m_pIUsbInputPipe || NULL == m_pIUsbOutputPipe)
        {
            hr = E_UNEXPECTED;
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Input or output pipe not found, returning %!HRESULT!",
                        hr
                        );        
        }
    }

    SAFE_RELEASE(pIUsbTargetFactory);

    return hr;
}

HRESULT
CMyDevice::ConfigureUsbPipes(
    )
/*++

Routine Description:

    This routine retrieves the IDs for the bulk end points of the USB device.

Arguments:

    None

Return Value:

    HRESULT
--*/
{
    HRESULT                 hr = S_OK;
    LONG                    timeout;

    //
    // Set timeout policies for input/output pipes
    //

    if (SUCCEEDED(hr)) 
    {
        timeout = ENDPOINT_TIMEOUT;

        hr = m_pIUsbInputPipe->SetPipePolicy(PIPE_TRANSFER_TIMEOUT,
                                             sizeof(timeout),
                                             &timeout);
        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Unable to set timeout policy for input pipe %!HRESULT!",
                        hr
                        );
        }
    }
        
    if (SUCCEEDED(hr)) 
    {
        timeout = ENDPOINT_TIMEOUT;

        hr = m_pIUsbOutputPipe->SetPipePolicy(PIPE_TRANSFER_TIMEOUT,
                                             sizeof(timeout),
                                             &timeout);
        if (FAILED(hr)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Unable to set timeout policy for output pipe %!HRESULT!",
                        hr
                        );
        }
    }

    return hr;
}

HRESULT
CMyDevice::IndicateDeviceReady(
    VOID
    )
/*++
 
  Routine Description:

    This method lights the period on the device's seven-segment display to 
    indicate that the driver's PrepareHardware method has completed.

  Arguments:

    None

  Return Value:

    Status

--*/
{
    SEVEN_SEGMENT display = {0};

    HRESULT hr;

    //
    // First read the contents of the seven segment display.
    //

    hr = GetSevenSegmentDisplay(&display);

    if (SUCCEEDED(hr)) 
    {
        display.Segments |= 0x08;

        hr = SetSevenSegmentDisplay(&display);
    }

    return hr;
}

HRESULT
CMyDevice::GetBarGraphDisplay(
    __in PBAR_GRAPH_STATE BarGraphState
    )
/*++
 
  Routine Description:

    This method synchronously retrieves the bar graph display information 
    from the OSR USB-FX2 device.  It uses the buffers in the FxRequest
    to hold the data it retrieves.

  Arguments:

    FxRequest - the request for the bar-graph info.

  Return Value:

    Status

--*/
{
    WINUSB_CONTROL_SETUP_PACKET setupPacket;

    ULONG bytesReturned;

    HRESULT hr = S_OK;

    //
    // Zero the contents of the buffer - the controller OR's in every
    // light that's set.
    //

    BarGraphState->BarsAsUChar = 0;

    //
    // Setup the control packet.
    //

    WINUSB_CONTROL_SETUP_PACKET_INIT( &setupPacket,
                                      BmRequestDeviceToHost,
                                      BmRequestToDevice,
                                      USBFX2LK_READ_BARGRAPH_DISPLAY,
                                      0,
                                      0 );

    //
    // Issue the request to WinUsb.
    //

    hr = SendControlTransferSynchronously(
                &(setupPacket.WinUsb),
                (PUCHAR) BarGraphState,
                sizeof(BAR_GRAPH_STATE),
                &bytesReturned
                );

    return hr;
}

HRESULT
CMyDevice::SetBarGraphDisplay(
    __in PBAR_GRAPH_STATE BarGraphState
    )
/*++
 
  Routine Description:

    This method synchronously sets the bar graph display on the OSR USB-FX2 
    device using the buffers in the FxRequest as input.

  Arguments:

    FxRequest - the request to set the bar-graph info.

  Return Value:

    Status

--*/
{
    WINUSB_CONTROL_SETUP_PACKET setupPacket;

    ULONG bytesTransferred;

    HRESULT hr = S_OK;

    //
    // Setup the control packet.
    //

    WINUSB_CONTROL_SETUP_PACKET_INIT( &setupPacket,
                                      BmRequestHostToDevice,
                                      BmRequestToDevice,
                                      USBFX2LK_SET_BARGRAPH_DISPLAY,
                                      0,
                                      0 );

    //
    // Issue the request to WinUsb.
    //

    hr = SendControlTransferSynchronously(
                &(setupPacket.WinUsb),
                (PUCHAR) BarGraphState,
                sizeof(BAR_GRAPH_STATE),
                &bytesTransferred
                );


    return hr;
}

HRESULT
CMyDevice::GetSevenSegmentDisplay(
    __in PSEVEN_SEGMENT SevenSegment
    )
/*++
 
  Routine Description:

    This method synchronously retrieves the bar graph display information 
    from the OSR USB-FX2 device.  It uses the buffers in the FxRequest
    to hold the data it retrieves.

  Arguments:

    FxRequest - the request for the bar-graph info.

  Return Value:

    Status

--*/
{
    WINUSB_CONTROL_SETUP_PACKET setupPacket;

    ULONG bytesReturned;

    HRESULT hr = S_OK;

    //
    // Zero the output buffer - the device will or in the bits for 
    // the lights that are set.
    //

    SevenSegment->Segments = 0;

    //
    // Setup the control packet.
    //

    WINUSB_CONTROL_SETUP_PACKET_INIT( &setupPacket,
                                      BmRequestDeviceToHost,
                                      BmRequestToDevice,
                                      USBFX2LK_READ_7SEGMENT_DISPLAY,
                                      0,
                                      0 );

    //
    // Issue the request to WinUsb.
    //

    hr = SendControlTransferSynchronously(
                &(setupPacket.WinUsb),
                (PUCHAR) SevenSegment,
                sizeof(SEVEN_SEGMENT),
                &bytesReturned
                );

    return hr;
}

HRESULT
CMyDevice::SetSevenSegmentDisplay(
    __in PSEVEN_SEGMENT SevenSegment
    )
/*++
 
  Routine Description:

    This method synchronously sets the bar graph display on the OSR USB-FX2 
    device using the buffers in the FxRequest as input.

  Arguments:

    FxRequest - the request to set the bar-graph info.

  Return Value:

    Status

--*/
{
    WINUSB_CONTROL_SETUP_PACKET setupPacket;

    ULONG bytesTransferred;

    HRESULT hr = S_OK;

    //
    // Setup the control packet.
    //

    WINUSB_CONTROL_SETUP_PACKET_INIT( &setupPacket,
                                      BmRequestHostToDevice,
                                      BmRequestToDevice,
                                      USBFX2LK_SET_7SEGMENT_DISPLAY,
                                      0,
                                      0 );

    //
    // Issue the request to WinUsb.
    //

    hr = SendControlTransferSynchronously(
                &(setupPacket.WinUsb),
                (PUCHAR) SevenSegment,
                sizeof(SEVEN_SEGMENT),
                &bytesTransferred
                );

    return hr;
}

HRESULT
CMyDevice::ReadSwitchState(
    __in PSWITCH_STATE SwitchState
    )
/*++
 
  Routine Description:

    This method synchronously retrieves the bar graph display information 
    from the OSR USB-FX2 device.  It uses the buffers in the FxRequest
    to hold the data it retrieves.

  Arguments:

    FxRequest - the request for the bar-graph info.

  Return Value:

    Status

--*/
{
    WINUSB_CONTROL_SETUP_PACKET setupPacket;

    ULONG bytesReturned;

    HRESULT hr = S_OK;

    //
    // Zero the output buffer - the device will or in the bits for 
    // the lights that are set.
    //

    SwitchState->SwitchesAsUChar = 0;

    //
    // Setup the control packet.
    //

    WINUSB_CONTROL_SETUP_PACKET_INIT( &setupPacket,
                                      BmRequestDeviceToHost,
                                      BmRequestToDevice,
                                      USBFX2LK_READ_SWITCHES,
                                      0,
                                      0 );

    //
    // Issue the request to WinUsb.
    //

    hr = SendControlTransferSynchronously(
                &(setupPacket.WinUsb),
                (PUCHAR) SwitchState,
                sizeof(SWITCH_STATE),
                &bytesReturned
                );

    return hr;
}

HRESULT
CMyDevice::SendControlTransferSynchronously(
    __in PWINUSB_SETUP_PACKET SetupPacket,
    __inout_ecount(BufferLength) PBYTE Buffer,
    __in ULONG BufferLength,
    __out PULONG LengthTransferred
    )
{
    HRESULT hr = S_OK;
    IWDFIoRequest *pWdfRequest = NULL;
    IWDFDriver * FxDriver = NULL;
    IWDFMemory * FxMemory = NULL; 
    IWDFRequestCompletionParams * FxComplParams = NULL;
    IWDFUsbRequestCompletionParams * FxUsbComplParams = NULL;

    *LengthTransferred = 0;
    
    hr = m_FxDevice->CreateRequest( NULL, //pCallbackInterface
                                    NULL, //pParentObject
                                    &pWdfRequest);

    if (SUCCEEDED(hr))
    {
        m_FxDevice->GetDriver(&FxDriver);

        hr = FxDriver->CreatePreallocatedWdfMemory( Buffer,
                                                    BufferLength,
                                                    NULL, //pCallbackInterface
                                                    pWdfRequest, //pParetObject
                                                    &FxMemory );
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pIUsbTargetDevice->FormatRequestForControlTransfer( pWdfRequest,
                                                                   SetupPacket,
                                                                   FxMemory,
                                                                   NULL); //TransferOffset
    }                                                          
                        
    if (SUCCEEDED(hr))
    {
        hr = pWdfRequest->Send( m_pIUsbTargetDevice,
                                WDF_REQUEST_SEND_OPTION_SYNCHRONOUS,
                                0); //Timeout
    }

    if (SUCCEEDED(hr))
    {
        pWdfRequest->GetCompletionParams(&FxComplParams);

        hr = FxComplParams->GetCompletionStatus();
    }

    if (SUCCEEDED(hr))
    {
        HRESULT hrQI = FxComplParams->QueryInterface(IID_PPV_ARGS(&FxUsbComplParams));
        WUDF_TEST_DRIVER_ASSERT(SUCCEEDED(hrQI));

        WUDF_TEST_DRIVER_ASSERT( WdfUsbRequestTypeDeviceControlTransfer == 
                            FxUsbComplParams->GetCompletedUsbRequestType() );

        FxUsbComplParams->GetDeviceControlTransferParameters( NULL,
                                                             LengthTransferred,
                                                             NULL,
                                                             NULL );
    }

    SAFE_RELEASE(FxUsbComplParams);
    SAFE_RELEASE(FxComplParams);
    SAFE_RELEASE(FxMemory);

    pWdfRequest->DeleteWdfObject();        
    SAFE_RELEASE(pWdfRequest);

    SAFE_RELEASE(FxDriver);

    return hr;
}

WDF_IO_TARGET_STATE
CMyDevice::GetTargetState(
    IWDFIoTarget * pTarget
    )
{
    IWDFIoTargetStateManagement * pStateMgmt = NULL;
    WDF_IO_TARGET_STATE state;

    HRESULT hrQI = pTarget->QueryInterface(IID_PPV_ARGS(&pStateMgmt));
    WUDF_TEST_DRIVER_ASSERT((SUCCEEDED(hrQI) && pStateMgmt));
    
    state = pStateMgmt->GetState();

    SAFE_RELEASE(pStateMgmt);
    
    return state;
}
    

VOID
CMyDevice::ServiceSwitchChangeQueue(
    __in SWITCH_STATE NewState,
    __in HRESULT CompletionStatus,
    __in_opt IWDFFile *SpecificFile
    )
/*++
 
  Routine Description:

    This method processes switch-state change notification requests as 
    part of reading the OSR device's interrupt pipe.  As each read completes
    this pulls all pending I/O off the switch change queue and completes
    each request with the current switch state.

  Arguments:

    NewState - the state of the switches

    CompletionStatus - all pending operations are completed with this status.

    SpecificFile - if provided only requests for this file object will get
                   completed.

  Return Value:

    None

--*/
{
    IWDFIoRequest *fxRequest;

    HRESULT enumHr = S_OK;

    do 
    {
        HRESULT hr;

        //
        // Get the next request.
        //

        if (NULL != SpecificFile)
        {
            enumHr = m_SwitchChangeQueue->RetrieveNextRequestByFileObject(
                                            SpecificFile,
                                            &fxRequest
                                            );
        }
        else
        {
            enumHr = m_SwitchChangeQueue->RetrieveNextRequest(&fxRequest);
        }

        //
        // if we got one then complete it.
        //

        if (SUCCEEDED(enumHr)) 
        {
            if (SUCCEEDED(CompletionStatus)) 
            {
                IWDFMemory *fxMemory;

                //
                // First copy the result to the request buffer.
                //

                fxRequest->GetOutputMemory(&fxMemory );

                hr = fxMemory->CopyFromBuffer(0, 
                                              &NewState, 
                                              sizeof(SWITCH_STATE));
                fxMemory->Release();
            }
            else 
            {
                hr = CompletionStatus;
            }

            //
            // Complete the request with the status of the copy (or the completion
            // status if that was an error).
            //

            if (SUCCEEDED(hr)) 
            {
                fxRequest->CompleteWithInformation(hr, sizeof(SWITCH_STATE));
            }
            else
            {
                fxRequest->Complete(hr);
            }

            fxRequest->Release();            
        }
    }
    while (SUCCEEDED(enumHr));
}


HRESULT
CMyDevice::OnD0Entry(
    __in IWDFDevice*  pWdfDevice,
    __in WDF_POWER_DEVICE_STATE  previousState
    )
{
    UNREFERENCED_PARAMETER(pWdfDevice);
    UNREFERENCED_PARAMETER(previousState);

    // 
    // Start/Stop the I/O target if you support a continuous reader.
    // The rest of the I/O is fed through power managed queues. The queue 
    // itself will stop feeding I/O to targets (and will wait for any pending 
    // I/O to complete before going into low power state), hence targets 
    // don’t need to be stopped/started. The continuous reader I/O is outside
    // of power managed queues so we need to Stop the I/O target on D0Exit and
    // start it on D0Entry. Please note that bulk pipe target doesn't need to 
    // be stopped/started because I/O submitted to this pipe comes from power 
    // managed I/O queue, which delivers I/O only in power on state.
    //
    
    WUDF_TEST_DRIVER_ASSERT(m_pIoTargetInterruptPipeStateMgmt != NULL);

    m_pIoTargetInterruptPipeStateMgmt->Start();

    return S_OK;
}

HRESULT
CMyDevice::OnD0Exit(
    __in IWDFDevice*  pWdfDevice,
    __in WDF_POWER_DEVICE_STATE  previousState
    )
{
    UNREFERENCED_PARAMETER(pWdfDevice);
    UNREFERENCED_PARAMETER(previousState);

    //
    // I/O target Stop allways succeedes
    //
    WUDF_TEST_DRIVER_ASSERT(m_pIoTargetInterruptPipeStateMgmt != NULL);

    m_pIoTargetInterruptPipeStateMgmt->Stop(WdfIoTargetCancelSentIo);

    return S_OK;
}

void
CMyDevice::OnSurpriseRemoval(
    __in IWDFDevice*  pWdfDevice
    )
{
    UNREFERENCED_PARAMETER(pWdfDevice); 
}


HRESULT
CMyDevice::OnQueryRemove(
    __in IWDFDevice*  pWdfDevice
    )
{
    UNREFERENCED_PARAMETER(pWdfDevice);
    return S_OK;
}

HRESULT
CMyDevice::OnQueryStop(
    __in IWDFDevice*  pWdfDevice
    )
{
    UNREFERENCED_PARAMETER(pWdfDevice);
    return S_OK;
}

HRESULT
CMyDevice::ConfigContReaderForInterruptEndPoint(
    VOID
    )
/*++

Routine Description:

    This routine configures a continuous reader on the
    interrupt endpoint. It's called from the PrepareHarware event.

Arguments:


Return Value:

    HRESULT value

--*/
{
    HRESULT hr, hrQI;
    IUsbTargetPipeContinuousReaderCallbackReadComplete *pOnCompletionCallback = NULL;
    IUsbTargetPipeContinuousReaderCallbackReadersFailed *pOnFailureCallback= NULL;
    IWDFUsbTargetPipe2 * pIUsbInterruptPipe2;

    hrQI = this->QueryInterface(IID_PPV_ARGS(&pOnCompletionCallback));
    WUDF_TEST_DRIVER_ASSERT((SUCCEEDED(hrQI) && pOnCompletionCallback));

    hrQI = this->QueryInterface(IID_PPV_ARGS(&pOnFailureCallback));
    WUDF_TEST_DRIVER_ASSERT((SUCCEEDED(hrQI) && pOnFailureCallback));

    hrQI = m_pIUsbInterruptPipe->QueryInterface(IID_PPV_ARGS(&pIUsbInterruptPipe2));
    WUDF_TEST_DRIVER_ASSERT((SUCCEEDED(hrQI) && pIUsbInterruptPipe2));

    //
    // Reader requests are not posted to the target automatically.
    // Driver must explictly call WdfIoTargetStart to kick start the
    // reader.  In this sample, it's done in D0Entry.
    // By defaut, framework queues two requests to the target
    // endpoint. Driver can configure up to 10 requests with the 
    // parameter CONCURRENT_READS
    //    
    hr = pIUsbInterruptPipe2->ConfigureContinuousReader( sizeof(m_SwitchStateBuffer), 
                                                          0,//header
                                                          0,//trailer
                                                          CONCURRENT_READS, 
                                                          NULL,
                                                          pOnCompletionCallback,
                                                          m_pIUsbInterruptPipe,
                                                          pOnFailureCallback
                                                          );
               
    if (FAILED(hr)) {
        TraceEvents(TRACE_LEVEL_ERROR, TEST_TRACE_DEVICE,
                    "OsrFxConfigContReaderForInterruptEndPoint failed %!HRESULT!",
                    hr);
    }

    SAFE_RELEASE(pOnCompletionCallback);
    SAFE_RELEASE(pOnFailureCallback);
    SAFE_RELEASE(pIUsbInterruptPipe2);

    return hr;
}


BOOL
CMyDevice::OnReaderFailure(
    IWDFUsbTargetPipe * pPipe,
    HRESULT hrCompletion
    )
{   
    UNREFERENCED_PARAMETER(pPipe);
    
    m_InterruptReadProblem = hrCompletion;
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TEST_TRACE_DEVICE, 
                "%!FUNC! Failure completed with %!HRESULT!",
                hrCompletion
                );
    
    ServiceSwitchChangeQueue(m_SwitchState, 
                             hrCompletion,
                             NULL);

    return TRUE;
}


VOID
CMyDevice::OnReaderCompletion(
    IWDFUsbTargetPipe * pPipe,
    IWDFMemory * pMemory,
    SIZE_T NumBytesTransferred,
    PVOID Context
    )
{        
    WUDF_TEST_DRIVER_ASSERT(pPipe ==  (IWDFUsbTargetPipe *)Context);

    //
    // Make sure that there is data in the read packet.  Depending on the device
    // specification, it is possible for it to return a 0 length read in
    // certain conditions.
    //

    if (NumBytesTransferred == 0) {
        TraceEvents(TRACE_LEVEL_WARNING, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Zero length read occured on the Interrupt Pipe's "
                    "Continuous Reader\n"
                    );
        return;
    }

    WUDF_TEST_DRIVER_ASSERT(NumBytesTransferred == sizeof(m_SwitchState));
    
    //
    // Get the switch state
    //
    
    PVOID pBuff = pMemory->GetDataBuffer(NULL);

    CopyMemory(&m_SwitchState, pBuff, sizeof(m_SwitchState));
    
        
    //
    // Satisfy application request for switch change notification
    //
    
    ServiceSwitchChangeQueue(m_SwitchState, 
                             S_OK,
                             NULL);

    //
    // Make sure that the request that got completed is the one that we reuse
    // Don't Delete the request because it gets reused
    //
}


