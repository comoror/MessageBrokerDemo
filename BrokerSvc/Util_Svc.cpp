#include <strsafe.h>
#include <Windows.h>
#include <thread>
#include <tchar.h>
#include "Util_Svc.h"

using namespace xsdk;

#define X_LOG_INFO(fmt, ...)	Log(_T("[Util_SVC][INFO]%hs(%d)!")##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define X_LOG_ERROR(fmt, ...)	Log(_T("[Util_SVC][ERR]%hs(%d)!")##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)

static const TCHAR* LOG_FILE = TEXT("C:\\ProgramData\\xsdk.log");

static void Log(LPCTSTR tszFormat, ...)
{
    va_list argList;
    va_start(argList, tszFormat);

    TCHAR tszBuffer[1024] = { 0 };
    _vstprintf_s(tszBuffer, tszFormat, argList);
    va_end(argList);

    FILE* pFile = NULL;
    _tfopen_s(&pFile, LOG_FILE, _T("a"));
    if (pFile)
    {
        SYSTEMTIME st = { 0 };
        GetLocalTime(&st);
        _ftprintf(pFile, _T("[%04d-%02d-%02d %02d:%02d:%02d.%03d]%s"),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tszBuffer);
        fclose(pFile);
    }

    OutputDebugString(tszBuffer);
}

// Service Control Functions
////////////////////////////////////////////////////////////////
//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
DWORD Util_Svc::DoInstallSvc(LPTSTR szSvcName, BOOL bIsAutoStart)
{
    DWORD dwError = 0;
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szUnquotedPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
    {
        dwError = GetLastError();
        X_LOG_ERROR("Cannot install service (%d)\n", dwError);
        return dwError;
    }

    // In case the path contains a space, it must be quoted so that
    // it is correctly interpreted. For example,
    // "d:\my share\myservice.exe" should be specified as
    // ""d:\my share\myservice.exe"".
    TCHAR szPath[MAX_PATH];
    StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenSCManager failed (%d)\n", dwError);
        return dwError;
    }

    // Create the service

    schService = CreateService(
        schSCManager,              // SCM database 
        szSvcName,                 // name of service 
        szSvcName,                 // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        bIsAutoStart ? SERVICE_AUTO_START : SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        dwError = GetLastError();
        X_LOG_ERROR("CreateService failed (%d)\n", dwError);
        CloseServiceHandle(schSCManager);
        return dwError;
    }
    else X_LOG_INFO("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return dwError;
}

//
// Purpose: 
//   Starts the service if possible.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
DWORD __stdcall Util_Svc::DoStartSvc(LPTSTR szSvcName)
{
    DWORD dwError = 0;
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenSCManager failed (%d)\n", dwError);
        return dwError;
    }

    // Get a handle to the service.

    schService = OpenService(
        schSCManager,         // SCM database 
        szSvcName,            // name of service 
        SERVICE_ALL_ACCESS);  // full access 

    if (schService == NULL)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenService failed (%d)\n", dwError);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Check the status in case the service is not stopped. 

    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // information level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // size needed if buffer is too small
    {
        dwError = GetLastError();
        X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Check if the service is already running. It would be possible 
    // to stop the service here, but for simplicity this example just returns. 

    if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
    {
        X_LOG_INFO("Cannot start the service because it is already running\n");
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return 0;
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    // Wait for the service to stop before attempting to start it.

    while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the status until the service is no longer stop pending. 

        if (!QueryServiceStatusEx(
            schService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // size needed if buffer is too small
        {
            dwError = GetLastError();
            X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return dwError;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                dwError = ERROR_TIMEOUT;
                X_LOG_ERROR("Timeout waiting for service to stop\n");
                CloseServiceHandle(schService);
                CloseServiceHandle(schSCManager);
                return dwError;
            }
        }
    }

    // Attempt to start the service.

    if (!StartService(
        schService,  // handle to service 
        0,           // number of arguments 
        NULL))      // no arguments 
    {
        dwError = GetLastError();
        X_LOG_ERROR("StartService failed (%d)\n", dwError);
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return dwError;
    }
    else X_LOG_INFO("Service start pending...\n");

    // Check the status until the service is no longer start pending. 

    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // info level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // if buffer too small
    {
        dwError = GetLastError();
        X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Save the tick count and initial checkpoint.

    DWORD dwRetryCount = 0;

    while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth the wait hint, but no less than 1 second and no 
        // more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the status again. 

        if (!QueryServiceStatusEx(
            schService,             // handle to service 
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // if buffer too small
        {
            dwError = GetLastError();
            X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
            break;
        }

        if (ssStatus.dwCurrentState == SERVICE_RUNNING)
			break;

        dwRetryCount++;
        if (dwRetryCount > 5)
		{
			dwError = ERROR_TIMEOUT;
            X_LOG_ERROR("Timeout waiting for service to start\n");
			break;
		}
    }

    // Determine whether the service is running.

    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
    {
        X_LOG_INFO("Service started successfully.\n");
        dwError = 0;
    }
    else
    {
        X_LOG_ERROR("Service not started. \n");
        X_LOG_ERROR("  Current State: %d\n", ssStatus.dwCurrentState);
        X_LOG_ERROR("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
        X_LOG_ERROR("  Check Point: %d\n", ssStatus.dwCheckPoint);
        X_LOG_ERROR("  Wait Hint: %d\n", ssStatus.dwWaitHint);
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return dwError;
}

static BOOL __stdcall StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService)
{
    DWORD i;
    DWORD dwBytesNeeded;
    DWORD dwCount;

    LPENUM_SERVICE_STATUS   lpDependencies = NULL;
    ENUM_SERVICE_STATUS     ess;
    SC_HANDLE               hDepService;
    SERVICE_STATUS_PROCESS  ssp;

    DWORD dwStartTime = GetTickCount();
    DWORD dwTimeout = 30000; // 30-second time-out

    // Pass a zero-length buffer to get the required buffer size.
    if (EnumDependentServices(schService, SERVICE_ACTIVE,
        lpDependencies, 0, &dwBytesNeeded, &dwCount))
    {
        // If the Enum call succeeds, then there are no dependent
        // services, so do nothing.
        return TRUE;
    }
    else
    {
        if (GetLastError() != ERROR_MORE_DATA)
            return FALSE; // Unexpected error

        // Allocate a buffer for the dependencies.
        lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

        if (!lpDependencies)
            return FALSE;

        __try {
            // Enumerate the dependencies.
            if (!EnumDependentServices(schService, SERVICE_ACTIVE,
                lpDependencies, dwBytesNeeded, &dwBytesNeeded,
                &dwCount))
                return FALSE;

            for (i = 0; i < dwCount; i++)
            {
                ess = *(lpDependencies + i);
                // Open the service.
                hDepService = OpenService(schSCManager,
                    ess.lpServiceName,
                    SERVICE_STOP | SERVICE_QUERY_STATUS);

                if (!hDepService)
                    return FALSE;

                __try {
                    // Send a stop code.
                    if (!ControlService(hDepService,
                        SERVICE_CONTROL_STOP,
                        (LPSERVICE_STATUS)&ssp))
                        return FALSE;

                    // Wait for the service to stop.
                    while (ssp.dwCurrentState != SERVICE_STOPPED)
                    {
                        Sleep(ssp.dwWaitHint);
                        if (!QueryServiceStatusEx(
                            hDepService,
                            SC_STATUS_PROCESS_INFO,
                            (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS),
                            &dwBytesNeeded))
                            return FALSE;

                        if (ssp.dwCurrentState == SERVICE_STOPPED)
                            break;

                        if (GetTickCount() - dwStartTime > dwTimeout)
                            return FALSE;
                    }
                }
                __finally
                {
                    // Always release the service handle.
                    CloseServiceHandle(hDepService);
                }
            }
        }
        __finally
        {
            // Always free the enumeration buffer.
            HeapFree(GetProcessHeap(), 0, lpDependencies);
        }
    }
    return TRUE;
}

//
// Purpose: 
//   Stops the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
DWORD __stdcall Util_Svc::DoStopSvc(LPTSTR szSvcName)
{
    DWORD dwError = 0;
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenSCManager failed (%d)\n", dwError);
        return dwError;
    }

    // Get a handle to the service.

    schService = OpenService(
        schSCManager,         // SCM database 
        szSvcName,            // name of service 
        SERVICE_STOP |
        SERVICE_QUERY_STATUS |
        SERVICE_ENUMERATE_DEPENDENTS);

    if (schService == NULL)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenService failed (%d)\n", dwError);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Make sure the service is not already stopped.

    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        dwError = GetLastError();
        X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
        goto stop_cleanup;
    }

    if (ssp.dwCurrentState == SERVICE_STOPPED)
    {
        X_LOG_INFO("Service is already stopped.\n");
        goto stop_cleanup;
    }

    // If a stop is pending, wait for it.

    while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
    {
        X_LOG_INFO("Service stop pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssp.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            dwError = GetLastError();
            X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
        {
            X_LOG_INFO("Service stopped successfully.\n");
            goto stop_cleanup;
        }

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            X_LOG_ERROR("Service stop timed out.\n");
            goto stop_cleanup;
        }
    }

    // If the service is running, dependencies must be stopped first.

    StopDependentServices(schSCManager, schService);

    // Send a stop code to the service.

    if (!ControlService(
        schService,
        SERVICE_CONTROL_STOP,
        (LPSERVICE_STATUS)&ssp))
    {
        dwError = GetLastError();
        X_LOG_ERROR("ControlService failed (%d)\n", dwError);
        goto stop_cleanup;
    }

    // Wait for the service to stop.

    while (ssp.dwCurrentState != SERVICE_STOPPED)
    {
        Sleep(ssp.dwWaitHint);
        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            dwError = GetLastError();
            X_LOG_ERROR("QueryServiceStatusEx failed (%d)\n", dwError);
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            dwError = ERROR_TIMEOUT;
            X_LOG_ERROR("Wait timed out\n");
            goto stop_cleanup;
        }
    }
    X_LOG_INFO("Service stopped successfully\n");
    dwError = 0;

stop_cleanup:
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return dwError;
}

//
// Purpose: 
//   Deletes a service from the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
DWORD __stdcall Util_Svc::DoDeleteSvc(LPTSTR szSvcName)
{
    DWORD dwError = 0;
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenSCManager failed (%d)\n", dwError);
        return dwError;
    }

    // Get a handle to the service.

    schService = OpenService(
        schSCManager,       // SCM database 
        szSvcName,          // name of service 
        DELETE);            // need delete access 

    if (schService == NULL)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenService failed (%d)\n", dwError);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Delete the service.

    if (!DeleteService(schService))
    {
        dwError = GetLastError();
        X_LOG_ERROR("DeleteService failed (%d)\n", dwError);
    }
    else
    {
        dwError = 0;
        X_LOG_INFO("Service deleted successfully\n");
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return dwError;
}

//ref: https://learn.microsoft.com/zh-cn/windows/win32/services/modifying-the-dacl-for-a-service
//
// Purpose: 
//   Updates the service DACL to grant start, stop, delete, and read
//   control access to the Guest account.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
DWORD __stdcall Util_Svc::DoUpdateSvcDacl(
    LPCTSTR             szSvcName,
    LPTSTR              szTrusteeName,
    DWORD               AccessPermissions,
    ACCESS_MODE         AccessMode)
{
    SC_HANDLE            schSCManager;
    SC_HANDLE            schService;

    EXPLICIT_ACCESS      ea;
    SECURITY_DESCRIPTOR  sd;
    PSECURITY_DESCRIPTOR psd = NULL;
    PACL                 pacl = NULL;
    PACL                 pNewAcl = NULL;
    BOOL                 bDaclPresent = FALSE;
    BOOL                 bDaclDefaulted = FALSE;
    DWORD                dwError = 0;
    DWORD                dwSize = 0;
    DWORD                dwBytesNeeded = 0;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenSCManager failed (%d)\n", dwError);
        return dwError;
    }

    // Get a handle to the service

    schService = OpenService(
        schSCManager,              // SCManager database 
        szSvcName,                 // name of service 
        READ_CONTROL | WRITE_DAC); // access

    if (schService == NULL)
    {
        dwError = GetLastError();
        X_LOG_ERROR("OpenService failed (%d)\n", dwError);
        CloseServiceHandle(schSCManager);
        return dwError;
    }

    // Get the current security descriptor.

    if (!QueryServiceObjectSecurity(schService,
        DACL_SECURITY_INFORMATION,
        &psd,           // using NULL does not work on all versions
        0,
        &dwBytesNeeded))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            dwSize = dwBytesNeeded;
            psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),
                HEAP_ZERO_MEMORY, dwSize);
            if (psd == NULL)
            {
                // Note: HeapAlloc does not support GetLastError.
                dwError = ERROR_OUTOFMEMORY;
                X_LOG_ERROR("HeapAlloc failed\n");
                goto dacl_cleanup;
            }

            if (!QueryServiceObjectSecurity(schService,
                DACL_SECURITY_INFORMATION, psd, dwSize, &dwBytesNeeded))
            {
                dwError = GetLastError();
                X_LOG_ERROR("QueryServiceObjectSecurity failed (%d)\n", dwError);
                goto dacl_cleanup;
            }
        }
        else
        {
            dwError = GetLastError();
            X_LOG_ERROR("QueryServiceObjectSecurity failed (%d)\n", dwError);
            goto dacl_cleanup;
        }
    }

    // Get the DACL.

    if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl,
        &bDaclDefaulted))
    {
        dwError = GetLastError();
        X_LOG_ERROR("GetSecurityDescriptorDacl failed(%d)\n", dwError);
        goto dacl_cleanup;
    }

    // Build the ACE.

    BuildExplicitAccessWithName(&ea, szTrusteeName,
        AccessPermissions,
        AccessMode,
        NO_INHERITANCE);

    dwError = SetEntriesInAcl(1, &ea, pacl, &pNewAcl);
    if (dwError != ERROR_SUCCESS)
    {
        X_LOG_ERROR("SetEntriesInAcl failed(%d)\n", dwError);
        goto dacl_cleanup;
    }

    // Initialize a new security descriptor.

    if (!InitializeSecurityDescriptor(&sd,
        SECURITY_DESCRIPTOR_REVISION))
    {
        dwError = GetLastError();
        X_LOG_ERROR("InitializeSecurityDescriptor failed(%d)\n", dwError);
        goto dacl_cleanup;
    }

    // Set the new DACL in the security descriptor.

    if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE))
    {
        dwError = GetLastError();
        X_LOG_ERROR("SetSecurityDescriptorDacl failed(%d)\n", dwError);
        goto dacl_cleanup;
    }

    // Set the new DACL for the service object.

    if (!SetServiceObjectSecurity(schService,
        DACL_SECURITY_INFORMATION, &sd))
    {
        dwError = GetLastError();
        X_LOG_ERROR("SetServiceObjectSecurity failed(%d)\n", dwError);
        goto dacl_cleanup;
    }
    else
    {
        dwError = 0;
        X_LOG_INFO("Service DACL updated successfully\n");
    }

dacl_cleanup:
    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);

    if (NULL != pNewAcl)
        LocalFree((HLOCAL)pNewAcl);
    if (NULL != psd)
        HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

    return dwError;
}

HANDLE Util_Svc::RegisterSvcStatusChange(LPCTSTR szSvcName, DWORD dwNotifyMask, PFN_SC_NOTIFY_CALLBACK pfnNotifyCallback)
{
    //create a new event
    HANDLE hSvcNotify = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hSvcNotify == NULL)
    {
        X_LOG_ERROR("CreateEvent failed (%d)\n", GetLastError());
        return NULL;
    }

    std::thread([=] {
        SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (schSCManager == NULL)
        {
            X_LOG_ERROR("OpenSCManager failed (%d)\n", GetLastError());
            return;
        }

        SC_HANDLE schService = OpenService(schSCManager, szSvcName, SERVICE_QUERY_STATUS);
        if (schService == NULL)
        {
            X_LOG_ERROR("OpenSCManager failed (%d)\n", GetLastError());
            CloseServiceHandle(schSCManager);
            return;
        }

        //register for service status change
        SERVICE_NOTIFY SvcNotify = { 0 };
        SvcNotify.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
        SvcNotify.pfnNotifyCallback = pfnNotifyCallback;
        //SvcNotify.pszServiceNames = szSvcName;
        SvcNotify.pContext = (PVOID)szSvcName;

        DWORD dwRet = 0;

        while (true)
        {
            dwRet = NotifyServiceStatusChange(schService, dwNotifyMask, &SvcNotify);
            if (ERROR_SUCCESS != dwRet)
            {
                if (dwRet == ERROR_SERVICE_MARKED_FOR_DELETE)
                {
                    X_LOG_INFO("Service is marked for delete\n");
                }
                else
                {
                    X_LOG_ERROR("NotifyServiceStatusChange failed (%d)\n", dwRet);
                }
                break;
            }

            dwRet = WaitForSingleObjectEx(hSvcNotify, INFINITE, TRUE);
            if (dwRet == WAIT_IO_COMPLETION)
            {
                X_LOG_INFO("WAIT_IO_COMPLETION, continue\n");
            }
            else
            {
                X_LOG_ERROR("WaitForMultipleObjectsEx event: (%d)\n", dwRet);
                break;
            }
        }

        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        }).detach();

    return hSvcNotify;
}

VOID Util_Svc::UnregisterSvcStatusChange(HANDLE hSvcNotify)
{
    CloseHandle(hSvcNotify);
}

PSC_NOTIFICATION_REGISTRATION Util_Svc::RegisterSvcStatusChange2(LPCTSTR szSvcName, 
    PSC_NOTIFICATION_CALLBACK pfnNotifyCallback,
    LPVOID pContext)
{
    //get SubscribeServiceChangeNotifications func from SecHost.dll
    HMODULE hSecHost = LoadLibrary(L"SecHost.dll");
    if (hSecHost == NULL)
    {
		X_LOG_ERROR("LoadLibrary failed (%d)\n", GetLastError());
		return NULL;
	}

    typedef DWORD(WINAPI *PFN_SubscribeServiceChangeNotifications)(SC_HANDLE, DWORD, PSC_NOTIFICATION_CALLBACK, PVOID, PSC_NOTIFICATION_REGISTRATION*);
    PFN_SubscribeServiceChangeNotifications pFnSubscribeServiceChangeNotifications = 
        (PFN_SubscribeServiceChangeNotifications)GetProcAddress(hSecHost, "SubscribeServiceChangeNotifications");
    if (pFnSubscribeServiceChangeNotifications == NULL)
    {
        X_LOG_ERROR("SubscribeServiceChangeNotifications not found\n");
        FreeLibrary(hSecHost);
        return NULL;
    }

    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    PSC_NOTIFICATION_REGISTRATION pSvcNotifyReg = NULL;
    do
    {
        schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (schSCManager == NULL)
        {
            X_LOG_ERROR("OpenSCManager failed (%d)\n", GetLastError());
            break;
        }

        schService = OpenService(schSCManager, szSvcName, SERVICE_QUERY_STATUS);
        if (schService == NULL)
        {
            X_LOG_ERROR("OpenSCManager failed (%d)\n", GetLastError());
            break;
        }

        DWORD dwRet = pFnSubscribeServiceChangeNotifications(schService,
            SC_EVENT_STATUS_CHANGE, 
            pfnNotifyCallback, pContext, &pSvcNotifyReg);
        if (ERROR_SUCCESS != dwRet)
        {
			X_LOG_ERROR("SubscribeServiceChangeNotifications failed (%d)\n", dwRet);
			break;
		}
        else
        {
            X_LOG_INFO("SubscribeServiceChangeNotifications success\n");
        }
    } while (FALSE);

    if (schService)
		CloseServiceHandle(schService);
    if (schSCManager)
        CloseServiceHandle(schSCManager);
    FreeLibrary(hSecHost);

    return pSvcNotifyReg;
}

VOID Util_Svc::UnregisterSvcStatusChange2(PSC_NOTIFICATION_REGISTRATION pSvcNotifyReg)
{
    //get UnsubscribeServiceChangeNotifications from SecHost.dll
    HMODULE hSecHost = LoadLibrary(L"SecHost.dll");
    if (hSecHost == NULL)
    {
        X_LOG_ERROR("LoadLibrary failed (%d)\n", GetLastError());
        return;
    }

    typedef DWORD(WINAPI *PFN_UnsubscribeServiceChangeNotifications)(PSC_NOTIFICATION_REGISTRATION);
    PFN_UnsubscribeServiceChangeNotifications pFnUnsubscribeServiceChangeNotifications = 
        (PFN_UnsubscribeServiceChangeNotifications)GetProcAddress(hSecHost, "UnsubscribeServiceChangeNotifications");
    if (pFnUnsubscribeServiceChangeNotifications == NULL)
    {
		X_LOG_ERROR("UnsubscribeServiceChangeNotifications not found\n");
		FreeLibrary(hSecHost);
		return;
	}

    if (pSvcNotifyReg)
    {
        pFnUnsubscribeServiceChangeNotifications(pSvcNotifyReg);
	}
    FreeLibrary(hSecHost);
}

LPCTSTR Util_Svc::GetSvcStateString(DWORD dwSvcState)
{
	switch (dwSvcState)
	{
	case SERVICE_STOPPED:
		return _T("SERVICE_STOPPED");
	case SERVICE_START_PENDING:
		return _T("SERVICE_START_PENDING");
	case SERVICE_STOP_PENDING:
		return _T("SERVICE_STOP_PENDING");
	case SERVICE_RUNNING:
		return _T("SERVICE_RUNNING");
	case SERVICE_CONTINUE_PENDING:
		return _T("SERVICE_CONTINUE_PENDING");
	case SERVICE_PAUSE_PENDING:
		return _T("SERVICE_PAUSE_PENDING");
	case SERVICE_PAUSED:
		return _T("SERVICE_PAUSED");
	default:
		return _T("Unknown State");
	}
}

LPCTSTR Util_Svc::GetSvcNotifyString(DWORD dwSvcNotify)
{
    switch (dwSvcNotify)
    {
    case SERVICE_NOTIFY_STOPPED:
        return _T("SERVICE_NOTIFY_STOPPED");
    case SERVICE_NOTIFY_START_PENDING:
        return _T("SERVICE_NOTIFY_START_PENDING");
    case SERVICE_NOTIFY_STOP_PENDING:
        		return _T("SERVICE_NOTIFY_STOP_PENDING");
    case SERVICE_NOTIFY_RUNNING:
        return _T("SERVICE_NOTIFY_RUNNING");
    case SERVICE_NOTIFY_CONTINUE_PENDING:
        return _T("SERVICE_NOTIFY_CONTINUE_PENDING");
    case SERVICE_NOTIFY_PAUSE_PENDING:
        return _T("SERVICE_NOTIFY_PAUSE_PENDING");
    case SERVICE_NOTIFY_PAUSED:
        return _T("SERVICE_NOTIFY_PAUSED");
    case SERVICE_NOTIFY_CREATED:
        return _T("SERVICE_NOTIFY_CREATED");
    case SERVICE_NOTIFY_DELETED:
        return _T("SERVICE_NOTIFY_DELETED");
    case SERVICE_NOTIFY_DELETE_PENDING:
        return _T("SERVICE_NOTIFY_DELETE_PENDING");
    default:
        return _T("Unknown Notify");
    }
}

