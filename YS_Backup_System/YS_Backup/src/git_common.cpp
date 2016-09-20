#include "git_common.hpp"

namespace ys {
namespace git {

namespace core {

void 
initialize()
{
	git_libgit2_init();
	git_signature_now(&g_ys_signature, c_commit_author, c_commit_email);
}


void 
shutdown()
{
	git_signature_free(g_ys_signature);
	git_libgit2_shutdown();
}


git_repository*
central_init(const std::string& path)
{
	git_repository*		central;

	git_index*			index;
	git_oid				head_id, tree_id;
	git_tree*			tree;

	LG_CHCKD(
		git_repository_init(&central, path.c_str(), false));

	// Get the current index and the corresponding tree.
	git_repository_index(&index, central);
	git_index_write_tree(&tree_id, index);
	git_tree_lookup(&tree, central, &tree_id);

	LG_CHCKD(
		git_commit_create_v(&head_id, central, "HEAD",
							g_ys_signature, g_ys_signature,
							nullptr, "",
							tree, 0));

	git_index_free(index);
	git_tree_free(tree);

	return central;
}


ys_satellite
satellite_clone(const std::string& path,
				const std::string& central)
{
	ys_satellite	satellite;

	LG_CHCKD(
		git_clone(&satellite.repo, central.c_str(), path.c_str(), nullptr));

	git_remote_add_push(satellite.repo, "origin", 
						"refs/heads/master:refs/heads/master");
	git_remote_lookup(&satellite.origin, satellite.repo, "origin");

	return satellite;
}


ys_satellite
satellite_open(const std::string& path)
{
	ys_satellite	satellite;

	satellite.repo = repository_open(path);
	// NOTE: Perhaps we could also check remote lookup.
	git_remote_lookup(&satellite.origin, satellite.repo, "origin");

	satellite_fetch(satellite);

	return satellite;
}


void
satellite_fetch(ys_satellite& satellite)
{
	git_fetch_options	fetch_options = GIT_FETCH_OPTIONS_INIT;
	git_remote_fetch(satellite.origin, nullptr, &fetch_options, "fetch");
}


void
satellite_free(ys_satellite& satellite)
{
	git_remote_free(satellite.origin);
	git_repository_free(satellite.repo);
}


void
satellite_checkout(ys_satellite& satellite)
{
	git_reference*			merge_result_head;
	git_reference*			master_head_ref;
	git_oid					fetch_head_oid;

	// NOTE: Procedure to perform a fast-forward merge. 
	//		 - Retrieve fetch head oid and master head ref
	//		 - Change the master head ref target to point to fetch head oid
	//		 - Checkout head

	git_reference_lookup(&master_head_ref, satellite.repo, "refs/heads/master");
	git_reference_name_to_id(&fetch_head_oid, satellite.repo, "FETCH_HEAD");


	git_reference_set_target(&merge_result_head, master_head_ref,
							 &fetch_head_oid, "fast-forward checkout");


	git_checkout_options	checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_options.checkout_strategy = GIT_CHECKOUT_FORCE;
	git_checkout_head(satellite.repo, &checkout_options);


	git_reference_free(merge_result_head);
	git_reference_free(master_head_ref);
}


void
satellite_push(ys_satellite& satellite)
{
	// NOTE: git_remote_push doesn't work locally, UNLESS the remote is bare.
	//		 In order to acutally push data from a satellite to central, we have
	//		 to turn our remote into a bare repo, do the push, and turn it back.
	//		 Doing so doesn't seem to clear our central working dir, 
	//		 but it might, I don't know man.
	const char*		remote_url = git_remote_url(satellite.origin);
	git_repository*	remote_repo;

	git_repository_open(&remote_repo, remote_url);
	// NOTE: The following function is not declared in libgit header
	//		 despite being documented, so remember to add it to repository.h.
	git_repository_set_bare(remote_repo);


	// Push to our remote.
	git_strarray	push_refspecs = { 0 };
	git_remote_get_push_refspecs(&push_refspecs, satellite.origin);
	git_push_options	push_options = GIT_PUSH_OPTIONS_INIT;
	push_options.pb_parallelism = 0;

	LG_CHCKD(
		git_remote_push(satellite.origin, &push_refspecs, &push_options));


	// Turn remote back into a normal repository.
	LG_CHCKD(
		git_repository_set_workdir(remote_repo, remote_url, 0));

	// NOTE: libgit should set core.bare value to false, but it doesn't.
	git_config*		remote_config;
	git_repository_config(&remote_config, remote_repo);
	git_config_set_bool(remote_config, "core.bare", 0);
	git_config_free(remote_config);

	git_checkout_options	checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_options.checkout_strategy = GIT_CHECKOUT_FORCE;
	git_checkout_head(remote_repo, &checkout_options);

	git_repository_free(remote_repo);
}


git_repository*
repository_open(const std::string& path)
{
	git_repository*		repo;

	LG_CHCKD(
		git_repository_open(&repo, path.c_str()));

	return repo;
}


bool
repository_exists(const std::string& path)
{
	return git_repository_open_ext(nullptr, path.c_str(),
								   GIT_REPOSITORY_OPEN_NO_SEARCH, 
								   nullptr) == 0;
}


git_commit*
repository_head_commit(git_repository* repo)
{
	git_commit*		head_commit;
	git_oid			head_id;

	git_reference_name_to_id(&head_id, repo, "HEAD");
	git_commit_lookup(&head_commit, repo, &head_id);

	return head_commit;
}


git_annotated_commit*
repository_ann_head_commit(git_repository* repo)
{
	git_annotated_commit*	head_commit;
	git_oid					head_id;

	git_reference_name_to_id(&head_id, repo, "HEAD");
	git_annotated_commit_lookup(&head_commit, repo, &head_id);

	return head_commit;
}


bool
repository_needs_commit(git_repository* repo)
{
	bool	needs_commit = false;

	git_status_list*	statuses = nullptr;
	git_status_options	status_options = GIT_STATUS_OPTIONS_INIT;
	status_options.flags =
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

	LG_CHCKD(
		git_status_list_new(&statuses, repo, &status_options));
	size_t status_count = git_status_list_entrycount(statuses);

	needs_commit = (status_count > 0);

	git_status_list_free(statuses);

	return needs_commit;
}


void
repository_commit(git_repository* repo)
{
	git_commit*			head_commit = repository_head_commit(repo);

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
						  ys::git::callback::always_add, nullptr));

	git_index_write(index);
	git_index_write_tree(&tree_id, index);
	git_tree_lookup(&tree, repo, &tree_id);

	LG_CHCKD(
		git_commit_create(&commit_id, repo, "HEAD",
						  g_ys_signature, g_ys_signature,
						  "UTF-8", "",
						  tree, 1, parents));

	git_tree_free(tree);
	git_index_free(index);
	git_commit_free(head_commit);
}


} // namespace core

namespace callback {


int
always_add(const char* path,
		   const char* matched_pathspec,
		   void* payload)
{
	// NOTE: return 0 to add, < 0 to abort, and > 0 to skip
	return 0;
}


} // namespace callback

} // namespace git
} // namespace ys
