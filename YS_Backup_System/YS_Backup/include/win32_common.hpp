#ifndef __YS_WIN_FILE_HPP__
#define __YS_WIN_FILE_HPP__

#include <string>

namespace ys {
namespace win32 {

namespace file {

unsigned long long last_write_time(const std::string& path);

} // namespace file


namespace time {

std::string now_string();

} // namespace time

} // namespace win32
} // namespace ys


#endif // __YS_WIN_FILE_HPP__
