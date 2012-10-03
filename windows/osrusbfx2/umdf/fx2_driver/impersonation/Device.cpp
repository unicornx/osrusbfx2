/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    Device.cpp

Abstract:

    This module contains the implementation of the UMDF Skeleton sample driver's
    device callback object.

    The skeleton sample device does very little.  It does not implement either
    of the PNP interfaces so once the device is setup, it won't ever get any
    callbacks until the device is removed.

Environment:

   Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"
#include "initguid.h"
#include "usb_hw.h"

#include "device.tmh"

CMyDevice::~CMyDevice(
    )
{
}

HRESULT
CMyDevice::CreateInstance(
    __in IWDFDriver *FxDriver,
    __in IWDFDeviceInitialize * FxDeviceInit,
    __out PCMyDevice *Device
    )
/*++
 
  Routine Description:

    This method creates and initializs an instance of the skeleton driver's 
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
    // Configure things like the locking model before we go to create our 
    // partner device.
    //

    //
    // TODO: Set no locking so that we are able to cancel requests.
    //

    FxDeviceInit->SetLockingConstraint(None);

    //
    // TODO: If you're writing a filter driver then indicate that here. 
    //
    // FxDeviceInit->SetFilter();
    //
        
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
        m_FxDevice = fxDevice;

        //
        // Drop the reference we got from CreateDevice.  Since this object
        // is partnered with the framework object they have the same 
        // lifespan - there is no need for an additional reference.
        //

        fxDevice->Release();
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

    //
    // Get the bus type GUID for the device and confirm that we're attached to
    // USB.
    //
    // NOTE: Since this device only supports USB we'd normally trust our INF 
    // to ensure this.
    //
    // But if the device also supported 1394 then we could 
    // use this to determine which type of bus we were attached to.
    // 

    hr = GetBusTypeGuid();

    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Create the read-write queue.
    //

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

    Since the skeleton driver doesn't support any of the device events, this
    method simply calls the base class's BaseQueryInterface.

    If the skeleton is extended to include device event interfaces then this 
    method must be changed to check the IID and return pointers to them as
    appropriate.

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
                    "%!FUNC! Cannot get device name %!hresult!",
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
                        "%!FUNC! Cannot get device name %!hresult!",
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
    // Clear the seven segement display to indicate that we're done with 
    // prepare hardware.
    //

    if (SUCCEEDED(hr))
    {
        hr = IndicateDeviceReady();
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
    //
    // Delete USB Target Device WDF Object, this will in turn
    // delete all the children - interface and the pipe objects
    //
    // This makes sure that we remove USB target objects from object tree 
    // (and thereby free them) before any potential subsequent 
    // OnPrepareHardware creates new ones
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
                  if ( UsbdPipeTypeBulk == pIUsbPipe->GetType() )
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
    

HRESULT
CMyDevice::GetBusTypeGuid(
    VOID
    )
/*++
 
  Routine Description:

    This routine gets the device instance ID then invokes SetupDi to 
    retrieve the bus type guid for the device.  The bus type guid is 
    stored in object.

  Arguments:

    None

  Return Value:

    Status

--*/
{
    ULONG instanceIdCch = 0;
    PWSTR instanceId = NULL;

    HDEVINFO deviceInfoSet = NULL;
    SP_DEVINFO_DATA deviceInfo = {sizeof(SP_DEVINFO_DATA)};

    HRESULT hr;

    //
    // Retrieve the device instance ID.
    //

    hr = m_FxDevice->RetrieveDeviceInstanceId(NULL, &instanceIdCch);

    if (FAILED(hr))
    {
        goto Exit;
    }

    instanceId = new WCHAR[instanceIdCch];

    if (instanceId == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    hr = m_FxDevice->RetrieveDeviceInstanceId(instanceId, &instanceIdCch);

    if (FAILED(hr))
    {
        goto Exit2;
    }

    //
    // Call SetupDI to open the device info.
    //

    deviceInfoSet = SetupDiCreateDeviceInfoList(NULL, NULL);

    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit2;
    }

    if (SetupDiOpenDeviceInfo(deviceInfoSet,
                              instanceId,
                              NULL,
                              0,
                              &deviceInfo) == FALSE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit3;
    }

    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet,
                                         &deviceInfo,
                                         SPDRP_BUSTYPEGUID,
                                         NULL,
                                         (PBYTE) &m_BusTypeGuid,
                                         sizeof(m_BusTypeGuid),
                                         NULL) == FALSE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit3;
    }

Exit3:
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

Exit2:

    delete[] instanceId;

Exit:

    return hr;
}

HRESULT
CMyDevice::PlaybackFile(
    __in PFILE_PLAYBACK PlayInfo,
    __in IWDFIoRequest *FxRequest
    )
/*++
 
  Routine Description:

    This method impersonates the caller, opens the file and prints each 
    character to the seven segement display.

  Arguments:

    PlayInfo - the playback info from the request.
  
    FxRequest - the request (used for impersonation)

  Return Value:

    Status

--*/

{
    PLAYBACK_IMPERSONATION_CONTEXT context = {PlayInfo, NULL, S_OK};
    CComQIPtr<IWDFIoRequest2> fxRequest2 = FxRequest;
    HRESULT hr;

    //
    // Impersonate and open the playback file.
    //

    hr = FxRequest->Impersonate(
                        SecurityImpersonation,
                        this->QueryIImpersonateCallback(),
                        &context
                        );
    if (FAILED(hr))
    {
        goto exit;
    }

    //
    // Release the reference that was added in QueryIImpersonateCallback()
    //
    this->Release();

    hr = context.Hr;

    if (FAILED(hr))
    {
        goto exit;
    }
    
    //
    // Read from the file one character at a time until we hit 
    // EOF or the request is cancelled.
    //

    do
    {
        UCHAR c;
        ULONG bytesRead;

        //
        // Check for cancellation.
        //

        if (fxRequest2->IsCanceled())
        {
            hr = HRESULT_FROM_WIN32(ERROR_CANCELLED);
        }
        else
        {
            BOOL result;

            //
            // Read a character from the file and see if we can 
            // encode it on the display.
            //

            result = ReadFile(context.FileHandle,
                              &c,
                              sizeof(c),
                              &bytesRead,
                              NULL);

            if (result)
            {
                SEVEN_SEGMENT segment;
                BAR_GRAPH_STATE barGraph;

                if (bytesRead > 0)
                {
                    #pragma prefast(suppress:__WARNING_USING_UNINIT_VAR,"Above this->Release() method does not actually free 'this'")
                    if(EncodeSegmentValue(c, &segment) == true)
                    {
                        barGraph.BarsAsUChar = c;

                        SetSevenSegmentDisplay(&segment);
                        SetBarGraphDisplay(&barGraph);
                    }
                }
                else
                {
                    hr = S_OK;
                    break;
                }
            }
            else
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
        }

    } while(SUCCEEDED(hr));
    
    CloseHandle(context.FileHandle);

exit:

    return hr;
}

VOID
CMyDevice::OnImpersonate(
    __in PVOID Context
    )
/*++
 
  Routine Description:

    This routine handles the impersonation for the PLAY FILE I/O control.

  Arguments:

    Context - pointer to the impersonation context

  Return Value:

    None

--*/
{
    PPLAYBACK_IMPERSONATION_CONTEXT context;

    context = (PPLAYBACK_IMPERSONATION_CONTEXT) Context;

    context->FileHandle = CreateFile(context->PlaybackInfo->Path, 
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     NULL,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     NULL);

    if (context->FileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        context->Hr = HRESULT_FROM_WIN32(error);
    }
    else
    {
        context->Hr = S_OK;
    }

    return;
}

#define SS_LEFT (SS_TOP_LEFT | SS_BOTTOM_LEFT)
#define SS_RIGHT (SS_TOP_RIGHT | SS_BOTTOM_RIGHT)

bool
CMyDevice::EncodeSegmentValue(
    __in UCHAR Character,
    __out SEVEN_SEGMENT *SevenSegment
    )
{
    UCHAR letterMap[] = {
        (SS_TOP | SS_BOTTOM_LEFT | SS_RIGHT | SS_CENTER | SS_BOTTOM),   // a
        (SS_LEFT | SS_CENTER | SS_BOTTOM | SS_BOTTOM_RIGHT),            // b
        (SS_CENTER | SS_BOTTOM_LEFT | SS_BOTTOM),                       // c
        (SS_BOTTOM_LEFT | SS_CENTER | SS_BOTTOM | SS_RIGHT),            // d
        (SS_LEFT | SS_TOP | SS_CENTER | SS_BOTTOM),                     // e
        (SS_LEFT | SS_TOP | SS_CENTER),                                 // f
        (SS_TOP | SS_TOP_LEFT | SS_CENTER | SS_BOTTOM | SS_RIGHT),      // g
        (SS_LEFT | SS_RIGHT | SS_CENTER),                               // h
        (SS_BOTTOM_LEFT),                                               // i
        (SS_BOTTOM | SS_RIGHT),                                         // j
        (SS_LEFT | SS_CENTER | SS_BOTTOM),                              // k
        (SS_LEFT | SS_BOTTOM),                                          // l
        (SS_LEFT | SS_TOP | SS_RIGHT),                                  // m
        (SS_BOTTOM_LEFT | SS_CENTER | SS_BOTTOM_RIGHT),                 // n
        (SS_BOTTOM_LEFT | SS_BOTTOM_RIGHT | SS_CENTER | SS_BOTTOM),     // o
        (SS_LEFT | SS_TOP | SS_CENTER | SS_TOP_RIGHT),                  // p
        (SS_TOP_LEFT | SS_TOP | SS_CENTER | SS_RIGHT),                  // q
        (SS_BOTTOM_LEFT | SS_CENTER),                                   // r
        (SS_TOP_LEFT | 
         SS_TOP | SS_CENTER | SS_BOTTOM | 
         SS_BOTTOM_RIGHT),                                              // s
        (SS_TOP | SS_RIGHT),                                            // t
        (SS_LEFT | SS_RIGHT | SS_BOTTOM),                               // u
        (SS_BOTTOM_LEFT | SS_BOTTOM | SS_BOTTOM_RIGHT),                 // v
        (SS_LEFT | SS_BOTTOM | SS_BOTTOM_RIGHT),                        // w
        (SS_LEFT | SS_CENTER | SS_RIGHT),                               // x
        (SS_TOP_LEFT | SS_CENTER | SS_RIGHT),                           // y 
        (SS_TOP_RIGHT | 
         SS_TOP | SS_CENTER | SS_BOTTOM | 
         SS_BOTTOM_LEFT),                                               // z
    };

    UCHAR numberMap[] = {
        (SS_LEFT | SS_TOP | SS_BOTTOM | SS_RIGHT | SS_DOT),             // 0
        (SS_RIGHT | SS_DOT),                                            // 1
        (SS_TOP | 
         SS_TOP_RIGHT | SS_CENTER | SS_BOTTOM_LEFT | 
         SS_BOTTOM | SS_DOT),                                           // 2
        (SS_TOP | SS_CENTER | SS_BOTTOM | SS_RIGHT | SS_DOT),           // 3
        (SS_TOP_LEFT | SS_CENTER | SS_RIGHT | SS_DOT),                  // 4
        (SS_TOP_LEFT | 
         SS_TOP | SS_CENTER | SS_BOTTOM | 
         SS_BOTTOM_RIGHT | SS_DOT),                                     // 5
        (SS_TOP | SS_CENTER | SS_BOTTOM | 
         SS_LEFT | SS_BOTTOM_RIGHT | SS_DOT),                           // 6
        (SS_TOP | SS_RIGHT | SS_DOT),                                   // 7
        (SS_TOP | SS_BOTTOM | SS_CENTER | 
         SS_LEFT | SS_RIGHT | SS_DOT),                                  // 8
        (SS_TOP_LEFT | SS_TOP | SS_CENTER | SS_RIGHT | SS_DOT),         // 9
    };

    if (((Character >= 'a') && (Character <= 'z')) || 
        ((Character >= 'A') && (Character <= 'Z')))
    {
        SevenSegment->Segments = letterMap[tolower(Character) - 'a'];
        return true;
    }
    else if ((Character >= '0') && (Character <= '9'))
    {
        SevenSegment->Segments = numberMap[Character - '0'];
        return true;
    }
    else
    {
        return false;
    }
}



