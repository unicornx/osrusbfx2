/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:

    queue.h

Abstract:

    This file defines the queue callback interface.

Environment:

    Windows User-Mode Driver Framework (WUDF)

--*/

#pragma once


#define MAX_TRANSFER_SIZE(x)   64*1024*1024

//
// Queue Callback Object.
//

class CMyReadWriteQueue : 
    public IQueueCallbackRead,
    public IQueueCallbackWrite,
    public IRequestCallbackRequestCompletion,
    public IQueueCallbackIoStop,
    public CMyQueue
{
protected:    
    HRESULT
    Initialize(
        );

    void
    ForwardFormattedRequest(
        __in IWDFIoRequest*                         pRequest,
        __in IWDFIoTarget*                          pIoTarget
        );
    
public:

    CMyReadWriteQueue(
        __in PCMyDevice Device
        );

    virtual ~CMyReadWriteQueue();

    static 
    HRESULT 
    CreateInstance( 
        __in PCMyDevice Device,
        __out PCMyReadWriteQueue *Queue
        );

    HRESULT
    Configure(
        VOID
        )
    {
        return CMyQueue::Configure();
    }

    IQueueCallbackWrite *
    QueryIQueueCallbackWrite(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackWrite *>(this);
    }

    IQueueCallbackRead *
    QueryIQueueCallbackRead(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackRead *>(this);
    }

    IRequestCallbackRequestCompletion *
    QueryIRequestCallbackRequestCompletion(
        VOID
        )
    {
        AddRef();
        return static_cast<IRequestCallbackRequestCompletion *>(this);
    }

    IQueueCallbackIoStop*
    QueryIQueueCallbackIoStop(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackIoStop *>(this);
    }
        
    //
    // IUnknown
    //

    STDMETHOD_(ULONG,AddRef) (VOID) {return CUnknown::AddRef();}

    __drv_arg(this, __drv_freesMem(object))
    STDMETHOD_(ULONG,Release) (VOID) {return CUnknown::Release();}

    STDMETHOD_(HRESULT, QueryInterface)(
        __in REFIID InterfaceId, 
        __deref_out PVOID *Object
        );


    //
    // Wdf Callbacks
    //
    
    //
    // IQueueCallbackWrite
    //
    STDMETHOD_ (void, OnWrite)(
        __in IWDFIoQueue *pWdfQueue,
        __in IWDFIoRequest *pWdfRequest,
        __in SIZE_T NumOfBytesToWrite
        );

    //
    // IQueueCallbackRead
    //
    STDMETHOD_ (void, OnRead)(
        __in IWDFIoQueue *pWdfQueue,
        __in IWDFIoRequest *pWdfRequest,
        __in SIZE_T NumOfBytesToRead
        );

    //
    //IRequestCallbackRequestCompletion
    //
    
    STDMETHOD_ (void, OnCompletion)(
        __in IWDFIoRequest*                 pWdfRequest,
        __in IWDFIoTarget*                  pIoTarget,
        __in IWDFRequestCompletionParams*   pParams,
        __in PVOID                          pContext
        );

    //
    //IQueueCallbackIoStop
    //

    STDMETHOD_ (void, OnIoStop)(
        __in IWDFIoQueue *   pWdfQueue,
        __in IWDFIoRequest * pWdfRequest,
        __in ULONG           ActionFlags
        );
    
};

