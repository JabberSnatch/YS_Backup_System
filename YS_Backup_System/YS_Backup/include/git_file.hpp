#ifndef __YS_GIT_FILE_HPP__
#define __YS_GIT_FILE_HPP__

#include <string>


namespace ys {
namespace git {
namespace file {

enum ResolutionStyle
{
	kStyleDefault = 0,
	kStyleOurs,
	kStyleTheirs,

	kStyleCount
};

void file_resolve_conflicts(const std::string& path, ResolutionStyle style);

} // namespace file
} // namespace git
} // namespace ys


#endif // __YS_GIT_FILE_HPP__
