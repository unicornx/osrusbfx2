/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    DeviceNonPpo.cpp

Abstract:

    This module contains the implementation of the power management
    settings for the case where UMDF is the power policy owner.    

Environment:

   User Mode Driver Framework (UMDF)

--*/

#include "internal.h"
#include "initguid.h"
#include "usb_hw.h"

#include "devicePpo.tmh"

HRESULT
CMyDevice::SetPowerManagement(
    VOID
    )
/*++

  Routine Description:

    This method enables the idle and wake functionality
    using UMDF. UMDF has been set as the power policy
    owner (PPO) for the device stack and we are using power
    managed queues.

  Arguments:

    None

  Return Value:
    
    Status

--*/
{ 
    HRESULT hr;

    //
    // Enable USB selective suspend on the device.    
    // 
    
    hr = m_FxDevice->AssignS0IdleSettings( IdleUsbSelectiveSuspend,
                                PowerDeviceMaximum,
                                IDLE_TIMEOUT_IN_MSEC,
                                IdleAllowUserControl,
                                WdfUseDefault);                                                                                                   

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TEST_TRACE_DEVICE, 
                    "%!FUNC! Unable to assign S0 idle settings for the device %!HRESULT!",
                    hr
                    );
    }

    //
    // Enable Sx wake settings
    //

    if (SUCCEEDED(hr))
    {
        hr = m_FxDevice->AssignSxWakeSettings( PowerDeviceMaximum,
                                    WakeAllowUserControl,
                                    WdfUseDefault);
                                    
        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TEST_TRACE_DEVICE, 
                        "%!FUNC! Unable to set Sx Wake Settings for the device %!HRESULT!",
                        hr
                        );
        }
        
    }


    return hr;
}
