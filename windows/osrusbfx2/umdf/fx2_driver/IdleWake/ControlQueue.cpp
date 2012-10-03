/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:

    ControlQueue.cpp

Abstract:

    This file implements the I/O queue interface and performs
    the ioctl operations.

Environment:

    Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"

#include "winioctl.h"

#include "ControlQueue.tmh"

CMyControlQueue::CMyControlQueue(
    __in PCMyDevice Device
    ) : CMyQueue(Device)
{

}

HRESULT
STDMETHODCALLTYPE
CMyControlQueue::QueryInterface(
    __in REFIID InterfaceId,
    __out PVOID *Object
    )
/*++

Routine Description:


    Query Interface

Aruments:
    
    Follows COM specifications

Return Value:

    HRESULT indicatin success or failure

--*/
{
    HRESULT hr;


    if (IsEqualIID(InterfaceId, __uuidof(IQueueCallbackDeviceIoControl))) 
    {
        hr = S_OK;
        *Object = QueryIQueueCallbackDeviceIoControl(); 

    } 
    else 
    {
        hr = CMyQueue::QueryInterface(InterfaceId, Object);
    }

    return hr;
}

//
// Initialize 
//

HRESULT 
CMyControlQueue::CreateInstance(
    __in PCMyDevice Device,
    __out PCMyControlQueue *Queue
    )
/*++

Routine Description:


    CreateInstance creates an instance of the queue object.

Aruments:
    
    ppUkwn - OUT parameter is an IUnknown interface to the queue object

Return Value:

    HRESULT indicatin success or failure

--*/
{
    PCMyControlQueue queue = NULL;
    HRESULT hr = S_OK;

    queue = new CMyControlQueue(Device);

    if (NULL == queue)
    {
        hr = E_OUTOFMEMORY;
    }

    //
    // Call the queue callback object to initialize itself.  This will create 
    // its partner queue framework object.
    //

    if (SUCCEEDED(hr))
    {
        hr = queue->Initialize();
    }

    if (SUCCEEDED(hr)) 
    {
        *Queue = queue;
    }
    else
    {
        SAFE_RELEASE(queue);
    }

    return hr;
}

HRESULT
CMyControlQueue::Initialize(
    VOID
    )
{
    HRESULT hr;
    bool powerManaged;
    
    //
    // First initialize the base class.  This will create the partner FxIoQueue
    // object and setup automatic forwarding of I/O controls.
    //

    //
    // The framework (UMDF) will not deliver a 
    // request to the driver that arrives on a power-managed queue, unless  
    // the device is in a powered-up state. If you receive a request on a 
    // power-managed queue after the device has idled out,
    // the framework will not be able to power-up and present the request
    // to the driver unless it is the power policy owner (PPO).
    // Hence if the driver is not the PPO and it sits above the PPO, 
    // it must not use power-managed queues
    // 

#if defined(_NOT_POWER_POLICY_OWNER_)
    // 
    // If WinUsb.sys is the PPO we should not use power managed
    // queues. This way the driver recieves a request even when the 
    // device has idled out. The driver then forwards it down to WinUsb.sys
    // If necessary WinUsb takes the required steps to power up the device.
    // 
    
    powerManaged = false;
#else
    //
    // If we are implementing Idle/Wake where UMDF is the 
    // power policy owner(PPO) we can use power managed queues. 
    // If the device has idled out it will be powered up by the 
    // framework when a request is recieved on such a queue.
    //
    
    powerManaged = true;
#endif  

    hr = __super::Initialize(WdfIoQueueDispatchSequential, 
                             false, 
                             powerManaged
                             );

    //
    // return the status.
    //

    return hr;
}

VOID
STDMETHODCALLTYPE
CMyControlQueue::OnDeviceIoControl(
    __in IWDFIoQueue *FxQueue,
    __in IWDFIoRequest *FxRequest,
    __in ULONG ControlCode,
    __in SIZE_T InputBufferSizeInBytes,
    __in SIZE_T OutputBufferSizeInBytes
    )
/*++

Routine Description:


    DeviceIoControl dispatch routine

Aruments:
    
    FxQueue - Framework Queue instance
    FxRequest - Framework Request  instance
    ControlCode - IO Control Code
    InputBufferSizeInBytes - Lenth of input buffer
    OutputBufferSizeInBytes - Lenth of output buffer

    Always succeeds DeviceIoIoctl
Return Value:

    VOID

--*/
{
    UNREFERENCED_PARAMETER(FxQueue);
    
    IWDFMemory *memory = NULL;
    PVOID buffer;

    SIZE_T bigBufferCb;

    ULONG information = 0;

    bool completeRequest = true;

    HRESULT hr = S_OK;

    switch (ControlCode)
    {
        case IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR:
        {
            //
            // Get the output buffer.
            //

            FxRequest->GetOutputMemory(&memory );

            //
            // request the descriptor.
            //

            ULONG bufferCb;

            //
            // Get the buffer address then release the memory object.
            // The memory object remains valid until the request is 
            // completed.
            //

            buffer = memory->GetDataBuffer(&bigBufferCb);
            memory->Release();

            if (bigBufferCb > ULONG_MAX)
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                break;
            }
            else
            {
                bufferCb = (ULONG) bigBufferCb;
            }

            hr = m_Device->GetUsbTargetDevice()->RetrieveDescriptor(                
                        USB_CONFIGURATION_DESCRIPTOR_TYPE,
                        0,
                        0,
                        &bufferCb,
                        (PUCHAR) buffer
                        );

            if (SUCCEEDED(hr))
            {
                information = bufferCb;
            }

            break;
        }

        case IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY:
        {
            //
            // Make sure the buffer is big enough to hold the result of the
            // control transfer.
            //

            if (OutputBufferSizeInBytes < sizeof(BAR_GRAPH_STATE))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                FxRequest->GetOutputMemory(&memory );
            }

            if (SUCCEEDED(hr)) 
            {
                buffer = memory->GetDataBuffer(&bigBufferCb);
                memory->Release();

                hr = m_Device->GetBarGraphDisplay((PBAR_GRAPH_STATE) buffer);
            }

            //
            // If that worked then record how many bytes of data we're 
            // returning.
            //

            if (SUCCEEDED(hr)) 
            {
                information = sizeof(BAR_GRAPH_STATE);
            }

            break;
        }

        case IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY:
        {
            //
            // Make sure the buffer is big enough to hold the input for the
            // control transfer.
            //

            if (InputBufferSizeInBytes < sizeof(BAR_GRAPH_STATE))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                FxRequest->GetInputMemory(&memory);
            }

            //
            // Get the data buffer and use it to set the bar graph on the
            // device.
            //

            if (SUCCEEDED(hr)) 
            {
                buffer = memory->GetDataBuffer(&bigBufferCb);
                memory->Release();

                hr = m_Device->SetBarGraphDisplay((PBAR_GRAPH_STATE) buffer);
            }

            break;
        }

        case IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY:
        {
            //
            // Make sure the buffer is big enough to hold the result of the
            // control transfer.
            //

            if (OutputBufferSizeInBytes < sizeof(SEVEN_SEGMENT))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                FxRequest->GetOutputMemory(&memory );
            }

            if (SUCCEEDED(hr)) 
            {
                buffer = memory->GetDataBuffer(&bigBufferCb);
                memory->Release();
                hr = m_Device->GetSevenSegmentDisplay((PSEVEN_SEGMENT) buffer);
            }

            //
            // If that worked then record how many bytes of data we're 
            // returning.
            //

            if (SUCCEEDED(hr)) 
            {
                information = sizeof(SEVEN_SEGMENT);
            }

            break;
        }

        case IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY:
        {
            //
            // Make sure the buffer is big enough to hold the input for the
            // control transfer.
            //

            if (InputBufferSizeInBytes < sizeof(SEVEN_SEGMENT))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                FxRequest->GetInputMemory(&memory );
            }

            //
            // Get the data buffer and use it to set the bar graph on the
            // device.
            //

            if (SUCCEEDED(hr)) 
            {
                buffer = memory->GetDataBuffer(&bigBufferCb);
                memory->Release();

                hr = m_Device->SetSevenSegmentDisplay((PSEVEN_SEGMENT) buffer);
            }
            break;
        }

        case IOCTL_OSRUSBFX2_READ_SWITCHES: 
        {
            //
            // Make sure the buffer is big enough to hold the input for the
            // control transfer.
            //

            if (OutputBufferSizeInBytes < sizeof(SWITCH_STATE))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                FxRequest->GetOutputMemory(&memory );
            }

            //
            // Get the data buffer and use it to set the bar graph on the
            // device.
            //

            if (SUCCEEDED(hr)) 
            {
                buffer = memory->GetDataBuffer(&bigBufferCb);
                memory->Release();

                hr = m_Device->ReadSwitchState((PSWITCH_STATE) buffer);
            }

            if (SUCCEEDED(hr)) 
            {
                information = sizeof(SWITCH_STATE);
            }

            break;
        }

        case IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE: 
        {
            //
            // Make sure the buffer is big enough to hold the switch
            // state.
            //

            if (OutputBufferSizeInBytes < sizeof(SWITCH_STATE))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            else
            {
                //
                // Forward the request to the switch state change queue.
                //

                hr = FxRequest->ForwardToIoQueue(
                                    m_Device->GetSwitchChangeQueue()
                                    );

                if (SUCCEEDED(hr))
                {
                    completeRequest = false;
                }
            }

            break;
        }

        case IOCTL_OSRUSBFX2_RESET_DEVICE:
        case IOCTL_OSRUSBFX2_REENUMERATE_DEVICE:
        {
            //
            // WinUSB does not allow us to reset or re-enumerate the device.
            // Return not-supported for the error in both of these cases.
            //

            hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            break;
        }


        default:
        {
            hr = HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
            break;
        }
    }

    if (completeRequest)
    {
        FxRequest->CompleteWithInformation(hr, information);
    }

    return;
}

