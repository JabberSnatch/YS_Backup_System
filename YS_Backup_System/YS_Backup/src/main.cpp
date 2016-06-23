#include <Windows.h>
#include <WinCred.h>
#include <strsafe.h>

#include "git2.h"

#include <string>
#include <iostream>
#include <assert.h>


#define PRINT_WINDOWS_ERROR(msg)\
{\
	std::cout << msg << " w/err " << GetLastError() << std::endl;\
	assert(!msg);\
}


// {9CA77349-841E-411B-BCF6-C1CAA357A491}
static const GUID	app_guid =
{ 0x9ca77349, 0x841e, 0x411b,{ 0xbc, 0xf6, 0xc1, 0xca, 0xa3, 0x57, 0xa4, 0x91 } };

const std::string	test_env_root = "./TestEnv/";





int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

void InstallService(PWSTR, PWSTR, DWORD);
void UninstallService(PWSTR);

int
main(int, char**)
{
	return WinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
}


int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{

	
	{
		InstallService(L"YS_BackupService",
					   L"YS_BackupService",
					   SERVICE_DEMAND_START);
	}

	NOTIFYICONDATA	notify_icon_data;
	if (0)
	{
		TCHAR		szTip[] = L"Backup System";
		TCHAR		szInfo[] = L"YOLOSQUAD";
		TCHAR		szInfoTitle[] = L"SWAG";

		notify_icon_data.cbSize = sizeof(NOTIFYICONDATA);
		notify_icon_data.hWnd = NULL;
		notify_icon_data.uFlags = NIF_GUID | NIF_ICON | NIF_TIP;
		//UINT  uCallbackMessage;
		notify_icon_data.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		StringCchCopy(notify_icon_data.szTip, 
					  sizeof(notify_icon_data.szTip), 
					  szTip);
		//DWORD dwState;
		//DWORD dwStateMask;
		StringCchCopy(notify_icon_data.szInfo, 
					  sizeof(notify_icon_data.szInfo), 
					  szInfo);
		notify_icon_data.uVersion = NOTIFYICON_VERSION_4;
		StringCchCopy(notify_icon_data.szInfoTitle, 
					  sizeof(notify_icon_data.szInfoTitle), 
					  szInfoTitle);
		notify_icon_data.dwInfoFlags = NIIF_USER;
		notify_icon_data.guidItem = app_guid;
		//HICON hBalloonIcon;

		Shell_NotifyIcon(NIM_ADD, &notify_icon_data);
	}

	if (0)
	{
		git_libgit2_init();

		git_repository*		test_repo;
		std::string			repo_path = test_env_root + ".git";
		git_repository_init(&test_repo, repo_path.c_str(), true);

		git_repository_free(test_repo);

		git_libgit2_shutdown();
	}

	if (0)
	{
		Shell_NotifyIcon(NIM_DELETE, &notify_icon_data);
	}

	return 0;
}


void
InstallService(PWSTR service_name,
			   PWSTR display_name,
			   DWORD start_type)
{
	TCHAR		path[MAX_PATH];
	SC_HANDLE	sc_manager = NULL;
	SC_HANDLE	service = NULL;

	if (GetModuleFileName(NULL, path, ARRAYSIZE(path)))
	{
		sc_manager = 
			OpenSCManager(NULL, NULL, 
						  SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
		
		if (!sc_manager)
		{
			TCHAR		username[CREDUI_MAX_USERNAME_LENGTH+1];
			TCHAR		password[CREDUI_MAX_PASSWORD_LENGTH+1];
			BOOL		save;
			DWORD		error;

			SecureZeroMemory(username, sizeof(username));
			SecureZeroMemory(password, sizeof(password));

			CREDUI_INFO cui;
			cui.cbSize = sizeof(CREDUI_INFO);
			cui.hwndParent = NULL;
			cui.pszMessageText = L"Admin privileges are requested to create a service.";
			cui.pszCaptionText = L"YS_BackupService creation";
			cui.hbmBanner = NULL;

			error = CredUIPromptForCredentials(
				&cui,
				L"localhost",
				NULL,
				GetLastError(),
				username,
				CREDUI_MAX_USERNAME_LENGTH+1,
				password,
				CREDUI_MAX_PASSWORD_LENGTH+1,
				&save,
				CREDUI_FLAGS_GENERIC_CREDENTIALS | 
				CREDUI_FLAGS_ALWAYS_SHOW_UI | 
				CREDUI_FLAGS_DO_NOT_PERSIST
			);

			if (!error)
			{
				sc_manager =
					OpenSCManager(
						NULL, NULL, 
						SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
				SecureZeroMemory(username, sizeof(username));
				SecureZeroMemory(password, sizeof(password));
			}
		}
		

		if (sc_manager)
		{
			service = CreateService(
				sc_manager,                 // SCManager database
				service_name,               // Name of service
				display_name,               // Name to display
				SERVICE_QUERY_STATUS,       // Desired access
				SERVICE_WIN32_OWN_PROCESS,  // Service type
				start_type,                 // Service start type
				SERVICE_ERROR_NORMAL,       // Error control type
				path,                       // Service's binary
				NULL,                       // No load ordering group
				NULL,                       // No tag identifier
				NULL,						// No dependencies
				NULL,						// No service running account
				NULL						// No password of the account
			);
			if (!service)
				PRINT_WINDOWS_ERROR("Service creation failed");
		}
		else
			PRINT_WINDOWS_ERROR("SCManager was not found");
	}
	else
		PRINT_WINDOWS_ERROR("Module file name not found");

	if (sc_manager)
		CloseServiceHandle(sc_manager);
	if (service)
		CloseServiceHandle(service);
}


void
UninstallService(PWSTR service_name)
{
	SC_HANDLE		sc_manager = NULL;
	SC_HANDLE		service = NULL;
	SERVICE_STATUS	service_status = {};

	sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	assert(sc_manager);

	service = OpenService(sc_manager, service_name, 
						  SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
	assert(service);

	assert(ControlService(service, SERVICE_CONTROL_STOP, &service_status));
	while (QueryServiceStatus(service, &service_status))
	{
		if (service_status.dwCurrentState == SERVICE_STOP_PENDING)
			Sleep(1000);
		else break;
	}

	assert(service_status.dwCurrentState == SERVICE_STOPPED);

	if (!DeleteService(service))
		PRINT_WINDOWS_ERROR(L"DeleteService failed");

	if (sc_manager)
		CloseServiceHandle(sc_manager);
	if (service)
		CloseServiceHandle(service);
}

