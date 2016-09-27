#include <Windows.h>
#include <WinCred.h>
#include <strsafe.h>

#include "git_common.hpp"
#include "git_file.hpp"

#include "git2.h"

#include <string>
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
			// [ ] 3_ Merge
			// [ ] 4_ Merge, resolve conflicts

			// Case 1
			if (analysis_out & GIT_MERGE_ANALYSIS_FASTFORWARD)
			{
				ys::git::core::satellite_checkout(satellite);
			}
			// Case 3 and 4
			else if (analysis_out & GIT_MERGE_ANALYSIS_NORMAL)
			{
				int i = 0;
				i += 1;

				git_index*			merge_index;
				git_commit*			satellite_commit = 
					ys::git::core::repository_head_commit(satellite.repo);
				git_commit*			remote_commit =
					ys::git::core::repository_head_commit(remote_repo);
				git_merge_options		merge_options = GIT_MERGE_OPTIONS_INIT;
				git_checkout_options	checkout_options = 
					GIT_CHECKOUT_OPTIONS_INIT;

				merge_options.file_flags = GIT_MERGE_FILE_STYLE_MERGE;
				checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;

				//LG_CHCKD(
				//	git_merge_commits(&merge_index, satellite.repo,
				//					  satellite_commit, remote_commit,
				//					  &merge_options));
			
				git_annotated_commit*	fetch_head;
				git_oid					fetch_head_oid;
				git_reference_name_to_id(&fetch_head_oid, satellite.repo, "FETCH_HEAD");
				git_annotated_commit_lookup(&fetch_head, satellite.repo, &fetch_head_oid);
				
				git_repository_fetchhead_foreach(
					satellite.repo,
					ys::git::callback::find_merge_branch,
					&fetch_head_oid);

				const git_annotated_commit* their_heads[] = { 
					fetch_head
				};        
				git_merge(satellite.repo, their_heads, 1, 
						  &merge_options, &checkout_options);
				git_repository_index(&merge_index, satellite.repo);


				{
					git_index_conflict_iterator*	conflict_ite;
					// TODO: First iteration on conflict resolution and test
					git_index_conflict_iterator_new(&conflict_ite, merge_index);

					const git_index_entry*	ancestor;
					const git_index_entry*	ours;
					const git_index_entry*	theirs;
					while (git_index_conflict_next(&ancestor, &ours, &theirs,
												   conflict_ite) != GIT_ITEROVER)
					{
						if (ours->mtime.seconds > theirs->mtime.seconds)
						{
							//ys::git::file::file_resolve_conflicts(
							//	satellite_path + ancestor->path, 
							//	ys::git::file::kStyleOurs
							//);
							git_index_add(merge_index, ours);
						}
						else
						{
							//ys::git::file::file_resolve_conflicts(
							//	satellite_path + ancestor->path,
							//	ys::git::file::kStyleTheirs
							//);
							git_index_add(merge_index, theirs);
						}

						// NOTE: Conflict resolution is not really handled by
						//		 libgit2. So we have to do it ourselves, by 
						//		 openning each file and rewrite it using only
						//		 relevant data.

						git_index_conflict_remove(merge_index, ours->path);
						git_index_write(merge_index);
					}

					// STAGE
					git_index_write(merge_index);
					// FREE
					git_index_conflict_iterator_free(conflict_ite);
				}

				// COMMIT
				git_oid				merge_commit_oid, merge_tree_oid;
				git_tree*			merge_tree;

				git_commit*			fetch_commit;
				git_commit_lookup(&fetch_commit, satellite.repo, 
								  &fetch_head_oid);

				git_signature*		signature;
				git_signature_now(&signature, ys::git::core::c_commit_author,
								  ys::git::core::c_commit_email);

				const git_commit*	merge_parents[] = { 
					satellite_commit, fetch_commit
				};


				git_index_write_tree(&merge_tree_oid, merge_index);
				git_tree_lookup(&merge_tree, satellite.repo, &merge_tree_oid);

 				LG_CHCKD(
					git_commit_create(&merge_commit_oid, satellite.repo, "HEAD",
									  signature, 
									  signature,
									  "UTF-8", "",
									  merge_tree, 2, merge_parents));

				git_signature_free(signature);
				// PUSH

				git_repository_state_cleanup(satellite.repo);
				ys::git::core::satellite_push(satellite);

				// NOTE: This conflict resolution should be alright.
#if 0
				{
					git_index*						local_index;
					git_index_conflict_iterator*	conflict_ite;
					git_repository_index(&local_index, repo);
					git_index_conflict_iterator_new(&conflict_ite, local_index);

					const git_index_entry*	ancestor;
					const git_index_entry*	ours;
					const git_index_entry*	theirs;
					while (git_index_conflict_next(&ancestor, &ours, &theirs,
												   conflict_ite) != GIT_ITEROVER)
					{
						if (ours->mtime.seconds > theirs->mtime.seconds)
							git_index_entry_stage(ours);
						else
							git_index_entry_stage(theirs);
					}

					git_index_conflict_iterator_free(conflict_ite);
					git_index_free(local_index);
					git_repository_state_cleanup(repo);
				}
#endif
			}
			// Case 0 and 2
			else if (analysis_out & GIT_MERGE_ANALYSIS_UP_TO_DATE)
			{
				// NOTE: Procedure 0 is not implemented yet, Case 0 and 2 share
				//		 the same procedure.

				ys::git::core::satellite_push(satellite);
				//ys::git::core::satellite_fetch(satellite);
			}
			// Default
			else if (analysis_out & GIT_MERGE_ANALYSIS_NONE)
			{
				ys::git::core::satellite_checkout(satellite);
			}


			if (0)
			{
				ys::git::core::satellite_push(satellite);

				// Fetching our central modified state is necessary.
				ys::git::core::satellite_fetch(satellite);
			}

			// NOTE: We still need to check if fast-forward is possible.
			//ys::git::core::satellite_checkout(satellite);
	
			ys::git::core::satellite_free(satellite);
		}
		else
			ys::git::core::satellite_clone(satellite_path, central_path);
	
	
		ys::git::core::shutdown();
	}

	if (0)
	{
		git_libgit2_init();
		
		git_signature*	signature;
		git_signature_now(&signature, "YS_Backup", "YS_Backup@notanemail.ys");

		// MAIN DRIVE REPOSITORY CREATION
		{
			std::string		repo_path = test_env_root + test_main_drive;

			git_repository* repo;
			bool			repo_exists;

			git_commit*		head_commit;

			repo_exists = git_repository_open_ext(nullptr, repo_path.c_str(),
												  GIT_REPOSITORY_OPEN_NO_SEARCH,
												  nullptr) == 0;
			if (repo_exists)
			{	// GET EXISTING REPO
				git_oid		head_id;
			
				LG_CHCKD(
					git_repository_open(&repo, repo_path.c_str()));

				git_reference_name_to_id(&head_id, repo, "HEAD");
				git_commit_lookup(&head_commit, repo, &head_id);
			}
			else
			{	// CREATE REPO AND MAKE FIRST COMMIT
				git_index*	index;
				git_oid		head_id, tree_id;
				git_tree*	tree;

				LG_CHCKD(
					git_repository_init(&repo, repo_path.c_str(), false));
				
				// Get the current index and the corresponding tree.
				git_repository_index(&index, repo);
				git_index_write_tree(&tree_id, index);
				git_tree_lookup(&tree, repo, &tree_id);

				LG_CHCKD(
					git_commit_create_v(&head_id, repo, "HEAD",
										signature, signature, 
										nullptr, "",
										tree, 0));
				git_commit_lookup(&head_commit, repo, &head_id);

				git_index_free(index);
				git_tree_free(tree);
			}

			// RUN SOME KIND OF GIT STATUS
			bool needs_commit = false;
			{
				git_status_options	status_options = GIT_STATUS_OPTIONS_INIT;
				status_options.flags = 
					GIT_STATUS_OPT_INCLUDE_UNTRACKED |
					GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

				git_status_list*	statuses = nullptr;
				
				git_status_list_new(&statuses, repo, &status_options);
				size_t status_count = git_status_list_entrycount(statuses);

				needs_commit = status_count > 0;

				git_status_list_free(statuses);
			}

			// COMMIT CHANGES TO CENTRAL
			if (needs_commit)
			{
				const git_commit*	parents[] = { head_commit };
				char*				paths[] = { "." };
				git_strarray		arr = { paths, 1 };
				git_index*			index;
				git_oid				commit_id, tree_id;
				git_tree*			tree;

				git_repository_index(&index, repo);

				LG_CHCKD(
					git_index_add_all(index, &arr, 
									  GIT_INDEX_ADD_DEFAULT, 
									  ys_git_add_all, nullptr));

				git_index_write(index);
				git_index_write_tree(&tree_id, index);
				git_tree_lookup(&tree, repo, &tree_id);

				LG_CHCKD(
					git_commit_create(&commit_id, repo, "HEAD",
										signature, signature,
										"UTF-8", "",
										tree, 1, parents));
				
				git_tree_free(tree);
				git_index_free(index);
			}
			
			git_repository_free(repo);
		} // MAIN REPOSITORY CREATION

		// SATELLITE DRIVES CLONE
		{
			std::string		repo_path = test_env_root + test_drive_A;

			git_repository* repo;
			bool			repo_exists;
				
			repo_exists = git_repository_open_ext(nullptr, repo_path.c_str(),
												  GIT_REPOSITORY_OPEN_NO_SEARCH,
												  nullptr) == 0;
			
			if (repo_exists)
			{
				// NOTE: If the repository exists, we have to pull modifications from remote.
				git_remote*				origin;

				LG_CHCKD(
					git_repository_open(&repo, repo_path.c_str()));

				git_remote_lookup(&origin, repo, "origin");
				
				git_fetch_options	fetch_options = GIT_FETCH_OPTIONS_INIT;
				git_remote_fetch(origin, nullptr, 
								 &fetch_options, "fetch");

				// NOTE: First we gather data relative to our fetch_head
				git_reference*			fetch_head_ref;
				git_annotated_commit*	fetch_head_commit;
				git_oid					fetch_head_oid;

				git_reference_lookup(&fetch_head_ref, repo, "FETCH_HEAD");
					
				git_annotated_commit_from_ref(&fetch_head_commit, repo,
												fetch_head_ref);
				// THIS IS WHERE WE COULD DO A MERGE ANALYSIS
				
				// IN CASE OF A FAST-FORWARD MERGE
				{
					// NOTE: Then we perform a fast-forward merge. 
					//		 - Change the master ref target
					//		 - Checkout head
					git_reference*			merge_result_head;
					git_reference*			master_head_ref;
					
					git_reference_lookup(&master_head_ref, repo, "refs/heads/master");

					git_oid_cpy(&fetch_head_oid, git_reference_target(fetch_head_ref));
					git_reference_set_target(&merge_result_head, master_head_ref,
											 &fetch_head_oid, "fast-forward");
					
					git_checkout_options	checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
					checkout_options.checkout_strategy = GIT_CHECKOUT_FORCE;
					git_checkout_head(repo, &checkout_options);

					git_reference_free(merge_result_head);
					git_reference_free(master_head_ref);
				}
				git_annotated_commit_free(fetch_head_commit);
				git_reference_free(fetch_head_ref);

				// NOTE: This conflict resolution should be alright.
				{
					git_index*						local_index;
					git_index_conflict_iterator*	conflict_ite;
					git_repository_index(&local_index, repo);
					git_index_conflict_iterator_new(&conflict_ite, local_index);
				
					const git_index_entry*	ancestor;
					const git_index_entry*	ours;
					const git_index_entry*	theirs;
					while (git_index_conflict_next(&ancestor, &ours, &theirs, 
						   conflict_ite) != GIT_ITEROVER)
					{
						if (ours->mtime.seconds > theirs->mtime.seconds)
							git_index_entry_stage(ours);
						else
							git_index_entry_stage(theirs);
					}
					
					git_index_conflict_iterator_free(conflict_ite);
					git_index_free(local_index);
					git_repository_state_cleanup(repo);
				}


				git_remote_free(origin);
			} // if (repo_exists)
			else
			{
				std::string		source_path = test_env_root + test_main_drive;

				LG_CHCKD(
					git_clone(&repo, source_path.c_str(), repo_path.c_str(), 
							  nullptr));
			}

			git_repository_free(repo);
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

