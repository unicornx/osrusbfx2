/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    DeviceNonPpo.cpp

Abstract:

    This module contains the implementation of the power management
    settings for the case where UMDF is not the power policy owner.
    In this case WinUsb.sys is the power policy owner.

Environment:

   User Mode Driver Framework (UMDF)

--*/

#include "internal.h"
#include "initguid.h"
#include "usb_hw.h"

#include "devicenonPpo.tmh"

HRESULT
CMyDevice::SetPowerManagement(
    VOID
    )
/*++

  Routine Description:

    This method enables the WinUSB.sys driver to power the device down when it is
    idle.
    
    NOTE: If you want to give the user control over the idle and wake settings
    (via the 'Power management' tab for your device in device manager)
    this can be done through registry settings in the INF which tell 
    WinUsb.sys to enable this.

  Arguments:

    None

  Return Value:
    
    Status

--*/
{
    ULONG value;

    HRESULT hr;

    //
    // Timeout value specified in internal.h.
    //

    value = IDLE_TIMEOUT_IN_MSEC;

    hr = m_pIUsbTargetDevice->SetPowerPolicy( SUSPEND_DELAY,
                                              sizeof(ULONG),
                                              (PVOID) &value );                                         

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Unable to set power policy (SUSPEND_DELAY) for the device %!HRESULT!",
                    hr
                    );
    }


    //
    // Finally enable auto-suspend.
    //

    if (SUCCEEDED(hr))
    {
        BOOL AutoSuspsend = TRUE;
    
        hr = m_pIUsbTargetDevice->SetPowerPolicy( AUTO_SUSPEND,
                                                  sizeof(BOOL),
                                                  (PVOID) &AutoSuspsend );     
    }

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Unable to set power policy (AUTO_SUSPEND) for the device %!HRESULT!",
                    hr
                    );
    }

    return hr;
}

