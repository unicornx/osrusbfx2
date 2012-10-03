/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:

    queue.cpp

Abstract:

    This file implements the I/O queue interface and performs
    the read/write/ioctl operations.

Environment:

    Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"
#include "queue.tmh"

VOID
CMyQueue::OnCompletion(
    IWDFIoRequest*                 pWdfRequest,
    IWDFIoTarget*                  pIoTarget,
    IWDFRequestCompletionParams*   pParams,
    PVOID                          pContext
    )
{
    UNREFERENCED_PARAMETER(pIoTarget);
    UNREFERENCED_PARAMETER(pContext);
    
    pWdfRequest->CompleteWithInformation(
        pParams->GetCompletionStatus(),
        pParams->GetInformation()
        );
}

void
CMyQueue::ForwardFormattedRequest(
    __in IWDFIoRequest*                         pRequest,
    __in IWDFIoTarget*                          pIoTarget
    )
{
    //
    //First set the completion callback
    //

    IRequestCallbackRequestCompletion * pCompletionCallback = NULL;    
    HRESULT hrQI = this->QueryInterface(IID_PPV_ARGS(&pCompletionCallback));
    WUDF_TEST_DRIVER_ASSERT(SUCCEEDED(hrQI) && (NULL != pCompletionCallback));
    
    pRequest->SetCompletionCallback(
        pCompletionCallback,
        NULL
        );

    pCompletionCallback->Release();
    pCompletionCallback = NULL;

    //
    //Send down the request
    //
    
    HRESULT hrSend = S_OK;
    hrSend = pRequest->Send(pIoTarget, 
                            0,  //flags
                            0); //timeout
    
    if (FAILED(hrSend))
    {
        pRequest->CompleteWithInformation(hrSend, 0);
    }

    return;
}


CMyQueue::CMyQueue(
    __in PCMyDevice Device
    ) : 
    m_FxQueue(NULL),
    m_Parent(Device)
{
}

//
// Queue destructor.
// Free up the buffer, wait for thread to terminate and 
//

CMyQueue::~CMyQueue(
    VOID
    )
/*++

Routine Description:


    IUnknown implementation of Release

Aruments:


Return Value:

    ULONG (reference count after Release)

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TEST_TRACE_QUEUE, 
                "%!FUNC! Entry"
                );

}


HRESULT
STDMETHODCALLTYPE
CMyQueue::QueryInterface(
    __in REFIID InterfaceId,
    __deref_out PVOID *Object
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


    if (IsEqualIID(InterfaceId, __uuidof(IQueueCallbackCreate))) 
    {
        hr = S_OK;
        *Object = QueryIQueueCallbackCreate(); 
    } 
    else if (IsEqualIID(InterfaceId, __uuidof(IQueueCallbackWrite))) 
    {
        hr = S_OK;
        *Object = QueryIQueueCallbackWrite(); 
    } 
    else if (IsEqualIID(InterfaceId, __uuidof(IQueueCallbackRead))) 
    {
        hr = S_OK;
        *Object = QueryIQueueCallbackRead(); 
    } 
    else if (IsEqualIID(InterfaceId, __uuidof(IQueueCallbackDeviceIoControl))) 
    {
        hr = S_OK;
        *Object = QueryIQueueCallbackDeviceIoControl(); 

    }
    else if (IsEqualIID(InterfaceId, __uuidof(IRequestCallbackRequestCompletion))) 
    {
        hr = S_OK;
        *Object = QueryIRequestCallbackRequestCompletion(); 

    }    
    else 
    {
        hr = CUnknown::QueryInterface(InterfaceId, Object);
    }

    return hr;
}

//
// Initialize 
//

HRESULT 
CMyQueue::CreateInstance(
    __in PCMyDevice Device,
    __out PCMyQueue *Queue
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
    PCMyQueue queue;
    HRESULT hr;

    queue = new CMyQueue(Device);

    if (NULL == queue)
    {
        hr = E_OUTOFMEMORY;
        goto Exit1;
    }

    //
    // Call the queue callback object to initialize itself.  This will create 
    // its partner queue framework object.
    //

    hr = queue->Initialize();

    if (SUCCEEDED(hr)) 
    {
        *Queue = queue;
    }
    else
    {
        queue->Release();
    }

Exit1:
    return hr;
}

HRESULT
CMyQueue::Initialize(
    VOID
    )
{
    IWDFIoQueue *fxQueue;
    HRESULT hr;

    //
    // Create the I/O Queue object.
    //

    {
        IUnknown *callback = QueryIUnknown();

        hr = m_Parent->GetFxDevice()->CreateIoQueue(
                                        callback,
                                        TRUE,
                                        WdfIoQueueDispatchParallel,
                                        TRUE,
                                        FALSE,
                                        &fxQueue
                                        );
        callback->Release();
    }

    if (FAILED(hr))
    {
        goto Exit1;
    }

    m_FxQueue = fxQueue;

    //
    // Release the creation reference on the queue.  This object will be 
    // destroyed before the queue so we don't need to have a reference out 
    // on it.
    //

    fxQueue->Release();

Exit1:

    return hr;
}

HRESULT
CMyQueue::Configure(
    VOID
    )
{
    return S_OK;
}

// CMyQueue

void
CMyQueue::OnCreateFile(
    __in IWDFIoQueue * /* pWdfQueue*/,
    __in IWDFIoRequest * pWdfRequest,
    __in IWDFFile *     /* pWdfFileObject */
    )
/*++

Routine Description:

    OnCreateFile dispatch routine. It gets invoked by WDF when
    the driver receives a Create request
    
Aruments:
    
    pWdfQueue - WDF queue dispatching the Create request
    pWdfRequest - Create request
    pWdfFileObject - WDF file object created for the Create request

Return Value:

    VOID

--*/
{
    //
    // Add reference to the pipe objects because we touch them during
    // OnCleanupFile.
    //
    // In case of surprise remove OnReleaseHarware may come before OnCleaupFile.
    //
    // OnReleaseHardware calls DeleteWdfObject on the USB target device which
    // will in turn cause dispose of input and output pipe objects.
    //
    // Without this additional reference pipe objects will get destroyed and
    // OnCleanupFile would be touching freed memory.
    //
    m_Parent->GetInputPipe()->AddRef();
    m_Parent->GetOutputPipe()->AddRef();

    pWdfRequest->Complete(S_OK);
}

void
CMyQueue::OnCleanupFile(
    __in IWDFFile*  pFileObject
    )
/*++

Routine Description:

    Cleanup helper routine. This gets invoked by the cleanup dispatch routine
    in CMyDevice::OnCleanupFile

Aruments:
    
    pFileObject - Framework fileobject instance

Return Value:

    VOID

--*/
{
    m_Parent->GetInputPipe()->CancelSentRequestsForFile(pFileObject);
    m_Parent->GetOutputPipe()->CancelSentRequestsForFile(pFileObject);

    //
    // Release the references taken by OnCreateFile
    //
    m_Parent->GetInputPipe()->Release();
    m_Parent->GetOutputPipe()->Release();    
        
    return;
}

STDMETHODIMP_ (void)
CMyQueue::OnDeviceIoControl(
        __in IWDFIoQueue * /* pWdfQueue */,
        __in IWDFIoRequest *pWdfRequest,
        __in ULONG /* ControlCode */,
        __in SIZE_T /* InputBufferSizeInBytes */,
        __in SIZE_T /* OutputBufferSizeInBytes */
        )
/*++

Routine Description:


    DeviceIoControl dispatch routine

Aruments:
    
    pWdfQueue - Framework Queue instance
    pWdfRequest - Framework Request  instance
    ControlCode - IO Control Code
    InputBufferSizeInBytes - Lenth of input buffer
    OutputBufferSizeInBytes - Lenth of output buffer

    Always succeeds DeviceIoIoctl
Return Value:

    VOID

--*/
{

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TEST_TRACE_QUEUE, 
                "%!FUNC!"
                );

    pWdfRequest->Complete(E_NOTIMPL);
    return;
}

STDMETHODIMP_ (void)
CMyQueue::OnWrite(
        __in IWDFIoQueue * /* pWdfQueue */,
        __in IWDFIoRequest *pWdfRequest,
        __in SIZE_T BytesToWrite
         )
/*++

Routine Description:


    Write dispatch routine
    IQueueCallbackWrite

Aruments:
    
    pWdfQueue - Framework Queue instance
    pWdfRequest - Framework Request  instance
    BytesToWrite - Lenth of bytes in the write buffer

    Allocate and copy data to local buffer
Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TEST_TRACE_QUEUE,
                "%!FUNC!: Queue %p Request %p BytesToTransfer %d\n",
                this,
                pWdfRequest,
                (ULONG)(ULONG_PTR)BytesToWrite
                );

    HRESULT hr = S_OK;
    IWDFMemory * pInputMemory = NULL;
    IWDFUsbTargetPipe * pOutputPipe = m_Parent->GetOutputPipe();

    pWdfRequest->GetInputMemory(&pInputMemory);
    
    hr = pOutputPipe->FormatRequestForWrite(
                                pWdfRequest,
                                NULL, //pFile
                                pInputMemory,
                                NULL, //Memory offset
                                NULL  //DeviceOffset
                                );

    if (FAILED(hr))
    {
        pWdfRequest->Complete(hr);
    }
    else
    {
        ForwardFormattedRequest(pWdfRequest, pOutputPipe);
    }

    SAFE_RELEASE(pInputMemory);
    
    return;
}

STDMETHODIMP_ (void)
CMyQueue::OnRead(
        __in IWDFIoQueue * /* pWdfQueue */,
        __in IWDFIoRequest *pWdfRequest,
        __in SIZE_T BytesToRead
         )
/*++

Routine Description:


    Read dispatch routine
    IQueueCallbackRead

Aruments:
    
    pWdfQueue - Framework Queue instance
    pWdfRequest - Framework Request  instance
    BytesToRead - Lenth of bytes in the read buffer

    Copy available data into the read buffer
Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TEST_TRACE_QUEUE,
                "%!FUNC!: Queue %p Request %p BytesToTransfer %d\n",
                this,
                pWdfRequest,
                (ULONG)(ULONG_PTR)BytesToRead
                );

    HRESULT hr = S_OK;
    IWDFMemory * pOutputMemory = NULL;

    pWdfRequest->GetOutputMemory(&pOutputMemory);
    
    hr = m_Parent->GetInputPipe()->FormatRequestForRead(
                                pWdfRequest,
                                NULL, //pFile
                                pOutputMemory,
                                NULL, //Memory offset
                                NULL  //DeviceOffset
                                );

    if (FAILED(hr))
    {
        pWdfRequest->Complete(hr);
    }
    else
    {
        ForwardFormattedRequest(pWdfRequest, m_Parent->GetInputPipe());
    }

    SAFE_RELEASE(pOutputMemory);
    
    return;
}

