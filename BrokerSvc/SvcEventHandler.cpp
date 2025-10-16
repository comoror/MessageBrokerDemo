#include <Windows.h>
#include <dbt.h>
#include <string>
#include "SvcEventHandler.h"
#include "Log.h"
#include "Common.h"

void OnPowerEvent(IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
)
{
    switch (dwEventType) {
    case PBT_APMPOWERSTATUSCHANGE:
    {
        //An application should process this event by calling the GetSystemPowerStatus
        // function to retrieve the current power status of the computer. In particular,
        // the application should check the ACLineStatus, BatteryFlag, BatteryLifeTime,
        // and BatteryLifePercent members of the SYSTEM_POWER_STATUS structure for any changes.

        //call GetSystemPowerStatus to get the power status
        SYSTEM_POWER_STATUS powerStatus;
        if (GetSystemPowerStatus(&powerStatus))
        {
            LOG_INFO("[PBT_APMPOWERSTATUSCHANGE]ACLineStatus: %d, BatteryFlag: %d, BatteryLifeTime: %d, BatteryLifePercent: %d",
                powerStatus.ACLineStatus, powerStatus.BatteryFlag, powerStatus.BatteryLifeTime, powerStatus.BatteryLifePercent);
        }
        else
        {
            LOG_ERROR("GetSystemPowerStatus failed with %d", GetLastError());
        }

        break;
    }
    case PBT_POWERSETTINGCHANGE:
    {
        POWERBROADCAST_SETTING* pPowerSetting = (POWERBROADCAST_SETTING*)lpEventData;
        if (pPowerSetting)
        {
            std::string strPowerSetting = "[PBT_POWERSETTINGCHANGE]";
            if (IsEqualGUID(GUID_ACDC_POWER_SOURCE, pPowerSetting->PowerSetting))
            {
                DWORD powerCondition = *(DWORD*)pPowerSetting->Data;
                switch (powerCondition)
                {
                case PoAc:
                {
                    strPowerSetting += "GUID_ACDC_POWER_SOURCE: PoAc";
                    break;
                }
                case PoDc:
                {
                    strPowerSetting += "GUID_ACDC_POWER_SOURCE: PoDc";
                    break;
                }
                case PoHot:
                {
                    strPowerSetting += "GUID_ACDC_POWER_SOURCE: PoHot";
                    break;
                }
                default:
                {
                    strPowerSetting += "GUID_ACDC_POWER_SOURCE: " + std::to_string(powerCondition);
                    break;
                }
                }
            }
            else if (IsEqualGUID(GUID_BATTERY_PERCENTAGE_REMAINING, pPowerSetting->PowerSetting))
            {
                //the current battery capacity remaining as a percentage from 0 through 100
                DWORD batteryPercentage = *(DWORD*)pPowerSetting->Data;
                strPowerSetting += "GUID_BATTERY_PERCENTAGE_REMAINING: " + std::to_string(batteryPercentage);
            }
            else if (IsEqualGUID(GUID_CONSOLE_DISPLAY_STATE, pPowerSetting->PowerSetting))
            {
                //a value from the MONITOR_DISPLAY_STATE enumeration
                DWORD displayState = *(DWORD*)pPowerSetting->Data;
                if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorOff)
                {
                    strPowerSetting += "GUID_CONSOLE_DISPLAY_STATE: The display is off";
                }
                else if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorOn)
                {
                    strPowerSetting += "GUID_CONSOLE_DISPLAY_STATE: The display is on";
                }
                else if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorDim)
                {
                    strPowerSetting += "GUID_CONSOLE_DISPLAY_STATE: The display is dimmed";
                }
            }
            else if (IsEqualGUID(GUID_GLOBAL_USER_PRESENCE, pPowerSetting->PowerSetting))
            {
                //a value from the USER_ACTIVITY_PRESENCE enumeration
                DWORD userPresence = *(DWORD*)pPowerSetting->Data;
                if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserPresent)
                {
                    strPowerSetting += "GUID_GLOBAL_USER_PRESENCE: The user is present";
                }
                else if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserNotPresent)
                {
                    strPowerSetting += "GUID_GLOBAL_USER_PRESENCE: The user is not present";
                }
                else if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserInactive)
                {
                    strPowerSetting += "GUID_GLOBAL_USER_PRESENCE: The user is not active";
                }
            }
            else if (IsEqualGUID(GUID_IDLE_BACKGROUND_TASK, pPowerSetting->PowerSetting))
            {
                strPowerSetting += "GUID_IDLE_BACKGROUND_TASK";
            }
            else if (IsEqualGUID(GUID_LIDSWITCH_STATE_CHANGE, pPowerSetting->PowerSetting))
            {
                //a DWORD that indicates the current lid state
                DWORD lidState = *(DWORD*)pPowerSetting->Data;
                if (lidState == 0)
                {
                    strPowerSetting += "GUID_LIDSWITCH_STATE_CHANGE: The lid is closed";
                }
                else
                {
                    strPowerSetting += "GUID_LIDSWITCH_STATE_CHANGE: The lid is opened";
                }
            }
            else if (IsEqualGUID(GUID_MONITOR_POWER_ON, pPowerSetting->PowerSetting))
            {
                //a DWORD that indicates the current monitor state
                DWORD monitorState = *(DWORD*)pPowerSetting->Data;
                if (monitorState == 0)
                {
                    strPowerSetting += "GUID_MONITOR_POWER_ON: The monitor is off";
                }
                else
                {
                    strPowerSetting += "GUID_MONITOR_POWER_ON: The monitor is on";
                }
            }
            else if (IsEqualGUID(GUID_POWER_SAVING_STATUS, pPowerSetting->PowerSetting))
            {
                //a DWORD that indicates battery saver state
                DWORD powerSavingState = *(DWORD*)pPowerSetting->Data;
                if (powerSavingState == 0)
                {
                    strPowerSetting += "GUID_POWER_SAVING_STATUS: Battery saver is off";
                }
                else
                {
                    strPowerSetting += "GUID_POWER_SAVING_STATUS: Battery saver is on";
                }
            }
            //else if (IsEqualGUID(GUID_ENERGY_SAVER_STATUS, pPowerSetting->PowerSetting))
            //{
            //  //a DWORD with values from the ENERGY_SAVER_STATUS enumeration
            //  // that indicate the current energy saver status
            //  DWORD energySaverStatus = *(DWORD*)pPowerSetting->Data;
            //  if (energySaverStatus == ENERGY_SAVER_STATUS::ENERGY_SAVER_OFF)
            //  {
            //      strPowerSetting += "GUID_ENERGY_SAVER_STATUS: Energy saver is off";
            //  }
            //  else if (energySaverStatus == ENERGY_SAVER_STATUS::ENERGY_SAVER_STANDARD)
            //  {
            //      strPowerSetting += "GUID_ENERGY_SAVER_STATUS: Energy saver is in standard mode";
            //  }
            //  else if (energySaverStatus == ENERGY_SAVER_STATUS::ENERGY_SAVER_HIGH_SAVINGS)
            //  {
            //      strPowerSetting += "GUID_ENERGY_SAVER_STATUS: Energy saver is in high savings mode";
            //  }
            //}
            else if (IsEqualGUID(GUID_POWERSCHEME_PERSONALITY, pPowerSetting->PowerSetting))
            {
                //a GUID that indicates the new active power scheme personality
                GUID powerSchemePersonality = *(GUID*)pPowerSetting->Data;
                if (IsEqualGUID(GUID_MIN_POWER_SAVINGS, powerSchemePersonality))
                {
                    strPowerSetting += "GUID_POWERSCHEME_PERSONALITY: High Performance";
                }
                else if (IsEqualGUID(GUID_MAX_POWER_SAVINGS, powerSchemePersonality))
                {
                    strPowerSetting += "GUID_POWERSCHEME_PERSONALITY: Power Saver";
                }
                else if (IsEqualGUID(GUID_TYPICAL_POWER_SAVINGS, powerSchemePersonality))
                {
                    strPowerSetting += "GUID_POWERSCHEME_PERSONALITY: Automatic";
                }
                else
                {
                    strPowerSetting += "GUID_POWERSCHEME_PERSONALITY: ";// +util_string::guid_to_string(powerSchemePersonality);
                }
            }
            else if (IsEqualGUID(GUID_SESSION_DISPLAY_STATUS, pPowerSetting->PowerSetting))
            {
                //a DWORD with a value from the MONITOR_DISPLAY_STATE enumeration
                DWORD displayState = *(DWORD*)pPowerSetting->Data;
                if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorOff)
                {
                    strPowerSetting += "GUID_SESSION_DISPLAY_STATUS: The display is off";
                }
                else if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorOn)
                {
                    strPowerSetting += "GUID_SESSION_DISPLAY_STATUS: The display is on";
                }
                else if (displayState == MONITOR_DISPLAY_STATE::PowerMonitorDim)
                {
                    strPowerSetting += "GUID_SESSION_DISPLAY_STATUS: The display is dimmed";
                }
            }
            else if (IsEqualGUID(GUID_SESSION_USER_PRESENCE, pPowerSetting->PowerSetting)) {
                //a DWORD with a value from the USER_ACTIVITY_PRESENCE enumeration
                DWORD userPresence = *(DWORD*)pPowerSetting->Data;
                if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserPresent) {
                    strPowerSetting += "GUID_SESSION_USER_PRESENCE: The user is present";
                }
                else if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserNotPresent) {
                    strPowerSetting += "GUID_SESSION_USER_PRESENCE: The user is not present";
                }
                else if (userPresence == USER_ACTIVITY_PRESENCE::PowerUserInactive) {
                    strPowerSetting += "GUID_SESSION_USER_PRESENCE: The user is not active";
                }
            }
            else if (IsEqualGUID(GUID_SYSTEM_AWAYMODE, pPowerSetting->PowerSetting)) {
                //a DWORD that indicates the current away-mode state
                DWORD awayModeState = *(DWORD*)pPowerSetting->Data;
                if (awayModeState == 0) {
                    strPowerSetting += "GUID_SYSTEM_AWAYMODE: Away mode is off";
                }
                else {
                    strPowerSetting += "GUID_SYSTEM_AWAYMODE: Away mode is on";
                }
            }

            LOG_INFO("%hs", strPowerSetting.c_str());
        }
        break;
    }
    case PBT_APMSUSPEND: {
        LOG_INFO("[PBT_APMSUSPEND]The system is suspending");
        break;
    }
    case PBT_APMRESUMEAUTOMATIC: {
        LOG_INFO("[PBT_APMRESUMEAUTOMATIC]The system is resuming automatically");
        break;
    }
    case PBT_APMRESUMESUSPEND:
    {
        LOG_INFO("[PBT_APMRESUMESUSPEND]The system is resuming from a low-power state by user input");
        break;
    }
    case PBT_APMBATTERYLOW:
    {
        LOG_INFO("[PBT_APMBATTERYLOW]The battery power is low");
        break;
    }
    case PBT_APMOEMEVENT:
    {
        LOG_INFO("[PBT_APMOEMEVENT]An OEM-defined event has occurred");
        break;
    }
    default:
    {
        LOG_INFO("Unknown power event: %d", (DWORD)dwEventType);
        break;
    }
    }
}

void OnSessionChange(IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
)
{
    WTSSESSION_NOTIFICATION* pSessionNotify = (WTSSESSION_NOTIFICATION*)lpEventData;

    IPCMessageType ipcMessageType = IPCMessageType::IPC_MESSAGE_INVALID;
    switch (dwEventType)
    {
    case WTS_SESSION_LOGON:
        LOG_INFO("Session, %d, WTS_SESSION_LOGON", pSessionNotify->dwSessionId);
        ipcMessageType = IPCMessageType::SESSION_LOGON;
        break;
    case WTS_SESSION_LOGOFF:
        LOG_INFO("Session, %d, WTS_SESSION_LOGOFF", pSessionNotify->dwSessionId);
        ipcMessageType = IPCMessageType::SESSION_LOGOFF;
        break;
    case WTS_SESSION_LOCK:
        LOG_INFO("Session, %d, WTS_SESSION_LOCK", pSessionNotify->dwSessionId);
        ipcMessageType = IPCMessageType::SESSION_LOCK;
        break;
    case WTS_SESSION_UNLOCK:
        LOG_INFO("Session, %d, WTS_SESSION_UNLOCK", pSessionNotify->dwSessionId);
        ipcMessageType = IPCMessageType::SESSION_UNLOCK;
        break;
    case WTS_SESSION_REMOTE_CONTROL:
        LOG_INFO("Session, %d, WTS_SESSION_REMOTE_CONTROL", pSessionNotify->dwSessionId);
        break;
    default:
        LOG_INFO("Session, %d, WTS_SESSION_REMOTE_CONTROL", pSessionNotify->dwSessionId);
        break;
    }
}

void OnDeviceEvent(IN DWORD dwEventType, IN LPVOID lpEventData, IN LPVOID lpContext)
{
    //
    // This is the actual message from the interface via Windows messaging.
    // This code includes some additional decoding for this particular device type
    // and some common validation checks.
    //
    // Note that not all devices utilize these optional parameters in the same
    // way. Refer to the extended information for your particular device type 
    // specified by your GUID.
    //
    PDEV_BROADCAST_DEVICEINTERFACE b = (PDEV_BROADCAST_DEVICEINTERFACE)lpEventData;

    // Output some messages to the window.
    switch (dwEventType)
    {
    case DBT_DEVICEARRIVAL:
        LOG_INFO("DBT_DEVICEARRIVAL: %ws", b->dbcc_name);

        break;
    case DBT_DEVICEREMOVECOMPLETE:
        LOG_INFO("DBT_DEVICEREMOVECOMPLETE: %ws", b->dbcc_name);
        break;
    case DBT_DEVNODES_CHANGED:
        LOG_INFO("DBT_DEVNODES_CHANGED: %ws", b->dbcc_name);
        break;
    default:
        LOG_INFO("Others:%d", dwEventType);
        break;
    }
}
