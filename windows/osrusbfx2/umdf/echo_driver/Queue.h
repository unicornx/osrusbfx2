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

class CMyQueue : 
    public IQueueCallbackCreate,
    public IQueueCallbackRead,
    public IQueueCallbackWrite,
    public IQueueCallbackDeviceIoControl,
    public IRequestCallbackRequestCompletion,
    public CUnknown
{
    //
    // Unreferenced pointer to the partner Fx device.
    //

    IWDFIoQueue *m_FxQueue;

    //
    // Unreferenced pointer to the parent device.
    //

    PCMyDevice m_Parent;

    HRESULT
    Initialize(
        VOID
        );

    void
    ForwardFormattedRequest(
        __in IWDFIoRequest*                         pRequest,
        __in IWDFIoTarget*                          pIoTarget
        );
    
public:

    CMyQueue(
        __in PCMyDevice Device
        );

    virtual ~CMyQueue();

    static 
    HRESULT 
    CreateInstance( 
        __in PCMyDevice Device,
        __out PCMyQueue *Queue
        );

    HRESULT
    Configure(
        VOID
        );

    IQueueCallbackCreate *
    QueryIQueueCallbackCreate(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackCreate *>(this);
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

    IQueueCallbackDeviceIoControl *
    QueryIQueueCallbackDeviceIoControl(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackDeviceIoControl *>(this);
    }

    IRequestCallbackRequestCompletion *
    QueryIRequestCallbackRequestCompletion(
        VOID
        )
    {
        AddRef();
        return static_cast<IRequestCallbackRequestCompletion *>(this);
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
    // Helper routine to do the cleanup. 
    // 
    //

    void OnCleanupFile( 
        __in IWDFFile *pWdfFileObject
        ); 


    //
    // Wdf Callbacks
    //

    //
    // IQueueCallbackCreate
    //    
    STDMETHOD_ (void, OnCreateFile)(
        __in IWDFIoQueue *   pWdfQueue,
        __in IWDFIoRequest * pWdfRequest,
        __in IWDFFile *      pWdfFileObject
        );

    //
    // IQueueCallbackDeviceIoControl
    //
    STDMETHOD_ (void, OnDeviceIoControl)( 
        __in IWDFIoQueue *pWdfQueue,
        __in IWDFIoRequest *pWdfRequest,
        __in ULONG ControlCode,
        __in SIZE_T InputBufferSizeInBytes,
        __in SIZE_T OutputBufferSizeInBytes
           );

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
        IWDFIoRequest*                 pWdfRequest,
        IWDFIoTarget*                  pIoTarget,
        IWDFRequestCompletionParams*   pParams,
        PVOID                          pContext
        );    
};

