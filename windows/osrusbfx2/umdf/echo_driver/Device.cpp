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

#include "device.tmh"


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
    // TODO: Set the locking model.  The skeleton uses device level
    //       locking, but you can choose "none" as well.
    //

    FxDeviceInit->SetLockingConstraint(WdfDeviceLevel);

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
    
    HRESULT hr;

    hr = CMyQueue::CreateInstance(this, &m_DefaultQueue);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = m_DefaultQueue->Configure();

    m_DefaultQueue->Release();

    if (SUCCEEDED(hr)) 
    {
        hr = m_FxDevice->CreateDeviceInterface(&GUID_DEVINTERFACE_OSRUSBFX2,
                                               NULL);
    }

    if (SUCCEEDED(hr)) 
    {
        hr = m_FxDevice->AssignDeviceInterfaceState(&GUID_DEVINTERFACE_OSRUSBFX2,
                                               NULL,
                                               TRUE);
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
    else if(IsEqualIID(InterfaceId, __uuidof(IFileCallbackCleanup))) 
    {    
        *Object = QueryIFileCallbackCleanup();
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
        goto Exit1;
    }

    //
    // Allocate the buffer
    //

    deviceName = new WCHAR[deviceNameCch];

    if (deviceName == NULL) {
        hr = E_OUTOFMEMORY;
        goto Exit1;
    }

    //
    // Get the actual name
    //

    hr = m_FxDevice->RetrieveDeviceName(deviceName, &deviceNameCch);

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Cannot get device name %!hresult!",
                    hr
                    );
        goto Exit2;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TEST_TRACE_DEVICE, 
                "%!FUNC! Device name %S",
                deviceName
                );

    //
    // Create USB I/O Targets and configure them
    //

    hr = CreateUsbIoTargets();
    if (FAILED(hr))
    {
        goto Exit2;
    }

    if (SUCCEEDED(hr)) 
    {
        hr = ConfigureUsbIoTargets();
    }

Exit2:
    delete[] deviceName;

Exit1:
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
        m_pIUsbTargetDevice = NULL;
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
                if ( pIUsbPipe->IsInEndPoint() && (UsbdPipeTypeBulk == pIUsbPipe->GetType()) )
                {
                    m_pIUsbInputPipe = pIUsbPipe;
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
CMyDevice::ConfigureUsbIoTargets(
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
    HRESULT                 hr;                 
    LONG                    timeout;
    ULONG                   length;

    length = sizeof(UCHAR);

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

    if (SUCCEEDED(hr)) 
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Speed - %x\n",
                    m_Speed
                    );
    }

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

void
CMyDevice::OnCleanupFile(
    __in IWDFFile * FxFile
    )
{
    m_DefaultQueue->OnCleanupFile(FxFile);
}

