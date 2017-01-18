#ifndef __YS_WIN_FILE_HPP__
#define __YS_WIN_FILE_HPP__

#include <string>

namespace ys {
namespace platform {
namespace file {


unsigned long long last_write_time(const std::string& path);


} // namespace file
} // namespace platform
} // namespace ys


#endif // __YS_WIN_FILE_HPP__
