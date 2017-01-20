#include "win32_common.hpp"

#include "windows.h"
#include <stdlib.h>

namespace ys {
namespace win32 {

namespace file {


unsigned long long
last_write_time(const std::string& path)
{
	wchar_t* wpath = new wchar_t[path.length() + 1]();

	MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED,
						path.c_str(), path.length(),
						wpath, path.length());

	HANDLE file_handle = CreateFile(wpath,
									GENERIC_READ,
									FILE_SHARE_READ,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);

	delete[] wpath;
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		CloseHandle(file_handle);
		return 0ull - 1ull;
	}

	FILETIME last_write_time;
	if (!GetFileTime(file_handle, NULL, NULL, &last_write_time))
	{
		CloseHandle(file_handle);
		return 0ull - 1ull;
	}
	CloseHandle(file_handle);

	ULARGE_INTEGER result;
	result.HighPart = last_write_time.dwHighDateTime;
	result.LowPart = last_write_time.dwLowDateTime;

	return result.QuadPart;
}

}


namespace time {

std::string
now_string()
{
	SYSTEMTIME system_time;
	GetSystemTime(&system_time);

	//GetDateFormatEx
	//GetTimeFormatEx(L"en", TIME_NOSECONDS, &system_time, L");
	
	return std::string();
}

}

} // namespace win32
} // namespace ys
