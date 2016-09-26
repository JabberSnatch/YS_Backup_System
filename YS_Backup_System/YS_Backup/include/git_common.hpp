#ifndef __YS_GIT_COMMON_HPP__
#define __YS_GIT_COMMON_HPP__

#include <string>
#include <iostream>
#include <cassert>

#include "git2.h"


#define LG_CHCKD(func) \
{\
	int error = func;\
	if (error < 0) {\
		const git_error* e = giterr_last();\
		std::cout << "Git error : " << error << e->klass << " : " << e->message << std::endl;\
		assert(!"Git raised an error, please check the logs");\
	}\
}


namespace ys {
namespace git {

namespace core {

struct ys_satellite
{
	git_repository*		repo;
	git_remote*			origin;
};


void	initialize();
void	shutdown();

// NOTE: As there should always be a single central available at all time,
//		 wouldn't it be a good idea to make it a global ?
git_repository*		central_init(const std::string& path);

ys_satellite		satellite_clone(const std::string& path, 
									const std::string& central);
// Opens a satellite repository, finds the origin remote, and fetches.
ys_satellite		satellite_open(const std::string& path);
void				satellite_fetch(ys_satellite& satellite);
void				satellite_free(ys_satellite& satellite);
// Updates the satellite working directory to the central state.
void				satellite_checkout(ys_satellite& satellite);
// Broadcasts the satellite's commits to its central.
void				satellite_push(ys_satellite& satellite);

git_repository*			repository_open(const std::string& path);
bool					repository_exists(const std::string& path);
git_commit*				repository_head_commit(git_repository* repo);
git_annotated_commit*	repository_ann_head_commit(git_repository* repo);
bool					repository_needs_commit(git_repository* repo);
void					repository_commit(git_repository* repo);
void					repository_checkout(git_repository* repo);


static git_signature*		g_ys_signature;
static const char			c_commit_author[] = "YS_Backup";
static const char			c_commit_email[] = "YS_Backup@notanemail.ys";


// FUTURE CANDIDATES:
bool				satellite_is_fast_forward(ys_satellite& satellite);
void				satellite_merge_with_origin(ys_satellite& satellite);

git_commit*			repository_get_commit(git_repository* repo,
										  const std::string&);

} // namespace core


namespace callback {

int always_add(const char* path, 
			   const char* matched_pathspec, 
			   void* payload);

// NOTE: payload is expected to be a pointer to git_oid
int find_merge_branch(const char* ref_name,
					  const char* remote_url,
					  const git_oid* oid,
					  unsigned int is_merge,
					  void* payload);

} // namespace callback

} // namespace git
} // namespace ys


#endif // __YS_GIT_COMMON_HPP__
