#pragma once

#include <aclapi.h>

namespace xsdk
{
	namespace Util_Svc
	{
		DWORD __stdcall DoInstallSvc(LPTSTR szSvcName, BOOL bIsAutoStart = FALSE);
		DWORD __stdcall DoStartSvc(LPTSTR szSvcName);
		DWORD __stdcall DoStopSvc(LPTSTR szSvcName);
		DWORD __stdcall DoDeleteSvc(LPTSTR szSvcName);
		DWORD __stdcall DoUpdateSvcDacl(
			LPCTSTR             szSvcName,
			LPTSTR              szTrusteeName,
			DWORD               AccessPermissions,
			ACCESS_MODE         AccessMode);

		HANDLE RegisterSvcStatusChange(LPCTSTR szSvcName, 
			DWORD dwNotifyMask, 
			PFN_SC_NOTIFY_CALLBACK pfnNotifyCallback);

		VOID UnregisterSvcStatusChange(HANDLE hSvcNotify);

		PSC_NOTIFICATION_REGISTRATION RegisterSvcStatusChange2(LPCTSTR szSvcName,
			PSC_NOTIFICATION_CALLBACK pfnNotifyCallback,
			LPVOID pContext);

		VOID UnregisterSvcStatusChange2(PSC_NOTIFICATION_REGISTRATION pSvcNotifyReg);

		LPCTSTR GetSvcStateString(DWORD dwSvcState);

		LPCTSTR GetSvcNotifyString(DWORD dwSvcNotify);
	};
};

