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


#define LG_CHCKD(func) assert(func >= 0);


// {9CA77349-841E-411B-BCF6-C1CAA357A491}
static const GUID	app_guid =
{ 0x9ca77349, 0x841e, 0x411b,{ 0xbc, 0xf6, 0xc1, 0xca, 0xa3, 0x57, 0xa4, 0x91 } };

const std::string	test_env_root = "./TestEnv/";

// NOTE: The main drive is where the repo should be first created.
const std::string	test_main_drive = "main/";
const std::string	test_drive_A = "A/";
const std::string	test_drive_B = "B/";



int ys_git_add_all(const char*, const char*, void*);


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
main(int, char**)
{
	return WinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
}


int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{

	{
		int ret_code = 0;
		git_libgit2_init();
		
		git_signature*	signature;
		ret_code = git_signature_now(&signature, 
									 "YS_Backup", 
									 "YS_Backup@notanemail.ys");

		// MAIN DRIVE REPOSITORY CREATION
		{
			std::string		repo_path = test_env_root + test_main_drive;
			git_repository* repo;
			git_index*		index;
			git_oid			tree_id, commit_id;
			git_tree*		tree;

			git_commit*		head_commit;

			bool			repo_exists;
			repo_exists = git_repository_open_ext(nullptr, repo_path.c_str(),
												  GIT_REPOSITORY_OPEN_NO_SEARCH,
												  nullptr) == 0;

			LG_CHCKD(git_repository_init(&repo, repo_path.c_str(), false));
			LG_CHCKD(git_repository_index(&index, repo));

			LG_CHCKD(git_index_write_tree(&tree_id, index));
			LG_CHCKD(git_tree_lookup(&tree, repo, &tree_id));

			// FIRST COMMIT
			if (!repo_exists)
			{
				ret_code = git_commit_create_v(&commit_id, repo, "HEAD", 
											   signature, signature, 
											   nullptr, "",
											   tree, 0);
				assert(ret_code >= 0);

				git_commit_lookup(&head_commit, repo, &commit_id);
			}
			else
			{
				git_oid		head_id;

				LG_CHCKD(git_reference_name_to_id(&head_id, repo, "HEAD"));
				git_commit_lookup(&head_commit, repo, &head_id);
			}

			// ANY OTHER COMMIT
			{
				// TODO: Make sure that we actually have files to commit.
				char*			paths[] = { "." };
				git_strarray	arr = { paths, 1 };
				ret_code = git_index_add_all(index, &arr, 
											 GIT_INDEX_ADD_DEFAULT, 
											 ys_git_add_all, nullptr);
				assert(ret_code >= 0);

				git_index_write(index);
				git_index_write_tree(&tree_id, index);
				git_tree_lookup(&tree, repo, &tree_id);

				const git_commit* parents[] = { head_commit };
				ret_code = git_commit_create(&commit_id, repo, "HEAD",
											 signature, signature,
											 "UTF-8", "",
											 tree, 1, parents);
				assert(ret_code >= 0);
			}
			
			git_index_free(index);
			git_repository_free(repo);
		}

		// SATELLITE DRIVES CLONE
		{
			std::string		repo_path = test_env_root + test_drive_A;
			git_repository* repo;
			git_remote*		origin;
			bool			repo_exists;
				
			repo_exists = git_repository_open_ext(nullptr, repo_path.c_str(),
												  GIT_REPOSITORY_OPEN_NO_SEARCH,
												  nullptr) == 0;
			if (!repo_exists)
			{
				std::string		source_path = test_env_root + test_main_drive;

				git_clone(&repo, source_path.c_str(), repo_path.c_str(), nullptr);
			}
			else
			{
				git_fetch_options	fetch_options = GIT_FETCH_OPTIONS_INIT;

				git_repository_open(&repo, repo_path.c_str());

				git_remote_lookup(&origin, repo, "origin");
				git_remote_fetch(origin, nullptr, 
								 &fetch_options, nullptr);
			
				// TODO: Update the local index according to the data we fetched.
				//		 FETCH_HEAD should hold the results.
			}
		}

		git_signature_free(signature);
		git_libgit2_shutdown();
	}

	if (0)
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

