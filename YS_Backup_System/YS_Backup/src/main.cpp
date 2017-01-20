#include <Windows.h>
#include <WinCred.h>
#include <strsafe.h>

#include "git_common.hpp"
#include "git_file.hpp"
#include "win32_common.hpp"

#include "git2.h"

#include <string>
#include <vector>
#include <iostream>
#include <assert.h>


#define PRINT_WINDOWS_ERROR(msg)\
{\
	std::cout << msg << " w/err " << GetLastError() << std::endl;\
	assert(!msg);\
}


#ifndef __YS_GIT_COMMON_HPP__
#define LG_CHCKD(func) \
{\
	int error = func;\
	if (error < 0) {\
		const git_error* e = giterr_last();\
		std::cout << "Git error : " << error << e->klass << " : " << e->message << std::endl;\
		assert(!"Git raised an error, please check the logs");\
	}\
}
#endif


// {9CA77349-841E-411B-BCF6-C1CAA357A491}
static const GUID	app_guid =
{ 0x9ca77349, 0x841e, 0x411b,{ 0xbc, 0xf6, 0xc1, 0xca, 0xa3, 0x57, 0xa4, 0x91 } };

const std::string	test_env_root = "./TestEnv/";

// NOTE: The main drive is where the repo should be first created.
const std::string	test_main_drive = "main/";
const std::string	test_drive_A = "A/";
const std::string	test_drive_B = "B/";



int ys_git_add_all(const char*, const char*, void*);
int ys_git_find_merge_branch(const char *ref_name,
							 const char *remote_url,
							 const git_oid *oid,
							 unsigned int is_merge,
							 void *payload);


int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

void InstallService(PWSTR, PWSTR, DWORD);
void UninstallService(PWSTR);


int
ys_git_add_all(const char* path,
			   const char* matched_pathspec,
			   void* payload)
{
	return 0;
}


int
ys_git_find_merge_branch(const char* ref_name, const char* remote_url,
						 const git_oid* oid, unsigned int is_merge,
						 void* payload)
{
	if (is_merge)
	{
		git_oid* p_result = (git_oid*)payload;
		memcpy(p_result->id, oid->id, GIT_OID_RAWSZ * sizeof(unsigned char));
		return 1;
	}

	return 0;
}


int
main(int, char**)
{
	return WinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
}


int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	{
		std::string		central_path = test_env_root + test_main_drive;
		std::string		satellite_path = test_env_root + test_drive_A;

		ys::git::core::initialize();

	
		git_repository*	central;
		if (ys::git::core::repository_exists(central_path))
			central = ys::git::core::repository_open(central_path);
		else
			central = ys::git::core::central_init(central_path);
	
		if (ys::git::core::repository_needs_commit(central))
			ys::git::core::repository_commit(central);
	
		git_repository_free(central);
	
		if (ys::git::core::repository_exists(satellite_path))
		{
			ys::git::core::ys_satellite	satellite = 
				ys::git::core::satellite_open(satellite_path);
	
			// Commit our satellite modifications.
			if (ys::git::core::repository_needs_commit(satellite.repo))
				ys::git::core::repository_commit(satellite.repo);


			// Merge analysis on central > satellite
			git_merge_analysis_t	analysis_out;
			git_merge_preference_t	preference_out;
			
			const char*		remote_url = git_remote_url(satellite.origin);
			git_repository*	remote_repo;
			git_repository_open(&remote_repo, remote_url);
			
			git_annotated_commit*	remote_head = 
				ys::git::core::repository_ann_head_commit(remote_repo);

			git_annotated_commit*	satellite_head =
				ys::git::core::repository_ann_head_commit(satellite.repo);

			{
				const git_annotated_commit* merge_heads[] = { remote_head };
				git_merge_analysis(&analysis_out, &preference_out,
									satellite.repo, merge_heads, 1);
			}

// NOTE: Cases :
			// 0_ Central, Satellite
			// 1_ *Central, Satellite
			// 2_ Central, *Satellite
			// 3_ *Central, *Satellite
			// 4_ *Central, *Satellite /!\

// NOTE: Procedures (Assuming both repos have already been commited) :
			// [ ] 0_ Do nothing
			// [X] 1_ Checkout Satellite
			// [X] 2_ Push Satellite
			// [X] 3_ Merge
			// [X] 4_ Merge, resolve conflicts

			// Case 1
			if (analysis_out & GIT_MERGE_ANALYSIS_FASTFORWARD)
			{
				ys::git::core::satellite_checkout(satellite);
			}
			// Case 3 and 4
			else if (analysis_out & GIT_MERGE_ANALYSIS_NORMAL)
			{
				ys::git::core::satellite_merge_with_remote(satellite);
			}
			// Case 0 and 2
			else if (analysis_out & GIT_MERGE_ANALYSIS_UP_TO_DATE)
			{
				ys::git::core::satellite_push(satellite);
				ys::git::core::satellite_fetch(satellite);
			}
			// Default
			else if (analysis_out & GIT_MERGE_ANALYSIS_NONE)
			{
				ys::git::core::satellite_checkout(satellite);
			}

			ys::git::core::satellite_free(satellite);
		}
		else
			ys::git::core::satellite_clone(satellite_path, central_path);
	
	
		ys::git::core::shutdown();
	}


	if (0)
	{
		InstallService(L"YS_BackupService",
					   L"YS_BackupService",
					   SERVICE_DEMAND_START);
	}

	if (0)
	{
		NOTIFYICONDATA	notify_icon_data;

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

