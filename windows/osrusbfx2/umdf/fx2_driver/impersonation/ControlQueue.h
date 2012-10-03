/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:

    ControlQueue.h

Abstract:

    This file defines the queue callback object for handling device I/O
    control requests.  This is a serialized queue.

Environment:

    Windows User-Mode Driver Framework (WUDF)

--*/

#pragma once

//
// Queue Callback Object.
//

class CMyControlQueue : public IQueueCallbackDeviceIoControl,
                        public CMyQueue
{
    HRESULT
    Initialize(
        VOID
        );

public:

    CMyControlQueue(
        __in PCMyDevice Device
        );

    virtual 
    ~CMyControlQueue(
        VOID
        )
    {
        return;
    }

    static 
    HRESULT 
    CreateInstance( 
        __in PCMyDevice Device,
        __out PCMyControlQueue *Queue
        );

    HRESULT
    Configure(
        VOID
        )
    {
        return S_OK;
    }
    
    IQueueCallbackDeviceIoControl *
    QueryIQueueCallbackDeviceIoControl(
        VOID
        )
    {
        AddRef();
        return static_cast<IQueueCallbackDeviceIoControl *>(this);
    }

    //
    // IUnknown
    //

    STDMETHOD_(ULONG,AddRef) (VOID) {return CUnknown::AddRef();}

    __drv_arg(this, __drv_freesMem(object))
    STDMETHOD_(ULONG,Release) (VOID) {return CUnknown::Release();}

    STDMETHOD_(HRESULT, QueryInterface)(
        __in REFIID InterfaceId, 
        __out PVOID *Object
        );

    //
    // Wdf Callbacks
    //
    
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
};



