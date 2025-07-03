//ref: https://learn.microsoft.com/zh-cn/windows/win32/services/

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include "worker.h"
#include "SvcEventHandler.h"
#include "Util_Svc.h"

using namespace xsdk;

TCHAR SVCNAME[] = TEXT("BrokerSvc");

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

DWORD SvcCtrlHandlerEx(
    IN DWORD dwControl,
    IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
);

VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportEvent(LPCTSTR);


//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        Util_Svc::DoInstallSvc(SVCNAME, TRUE);
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("start")) == 0)
    {
        Util_Svc::DoStartSvc(SVCNAME);
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("stop")) == 0)
    {
        Util_Svc::DoStopSvc(SVCNAME);
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("uninstall")) == 0)
    {
        Util_Svc::DoStopSvc(SVCNAME);
        Util_Svc::DoDeleteSvc(SVCNAME);
        return 0;
    }

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
    }
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register the handler function for the service
    //gSvcStatusHandle = RegisterServiceCtrlHandler(
    //    SVCNAME,
    //    SvcCtrlHandler);

    gSvcStatusHandle = RegisterServiceCtrlHandlerEx(
        SVCNAME,
        SvcCtrlHandlerEx,
        NULL);

    if (!gSvcStatusHandle)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    //Initialize all fields in the SERVICE_STATUS structure, 
    // ensuring that there are valid check-point and wait hint values for pending states. 
    // Use reasonable wait hints.
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.
    gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN /*| SERVICE_ACCEPT_PAUSE_CONTINUE*/
        | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SESSIONCHANGE;

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.

    worker_start();

    while (1)
    {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        worker_stop();

        //Do not attempt to perform any additional work after calling 
        // SetServiceStatus with SERVICE_STOPPED, because the service process can be terminated at any time.
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);

        return;
    }
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    //Do not register to accept controls while the status is SERVICE_START_PENDING 
    // or the service can crash. After initialization is completed, 
    // accept the SERVICE_CONTROL_STOP code.
    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    //else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    //Call this function with checkpoint and wait-hint values 
    // only if the service is making progress on the tasks related to the pending start, 
    // stop, pause, or continue operation. Otherwise, SCM cannot detect if your service is hung.
    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    // Handle the requested control code. 

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}

DWORD SvcCtrlHandlerEx(
    IN DWORD dwControl,
    IN DWORD dwEventType,
    IN LPVOID lpEventData,
    IN LPVOID lpContext
)
{
    // Handle the requested control code. 
    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        break;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    case SERVICE_CONTROL_POWEREVENT:
    {
        OnPowerEvent(dwEventType, lpEventData, lpContext);
    }
    break;

    case SERVICE_CONTROL_SESSIONCHANGE:
    {
        OnSessionChange(dwEventType, lpEventData, lpContext);
    }
    break;

    case SERVICE_CONTROL_DEVICEEVENT:
    {
        OnDeviceEvent(dwEventType, lpEventData, lpContext);
    }
    break;

    default:
        break;
    }

    return 0;
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPCTSTR szFunction)
{
    TCHAR Buffer[80];

    StringCchPrintf(Buffer, 80, TEXT("%s!%s failed with %d"), SVCNAME, szFunction, GetLastError());

    OutputDebugString(Buffer);
}