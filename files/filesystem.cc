
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "filesystem.h"
#include <system_error>
#include "ignore_unused_variable_warning.h"
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

using std::string;

namespace fs {

bool exists(
    const string& file,
    std::error_code& err) noexcept
{
    struct stat sbuf;
    err = {stat(file.data(), std::addressof(sbuf)),
        std::system_category()};
    return (err.value() == 0);
}

bool create_directory(
    const string& name,
    std::error_code& err) noexcept
{
    err = {0, std::system_category()};
#if defined(_WIN32) && defined(UNICODE)
	const char *n = name.c_str();
	int nLen = std::strlen(n) + 1;
	LPTSTR lpszT = (LPTSTR) alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	ignore_unused_variable_warning(mode);
    bool success;
	if(!CreateDirectory(lpszT, nullptr))
        err = {GetLastError(), std::system_category()};
#elif defined(_WIN32)
	ignore_unused_variable_warning(mode);
	err = {mkdir(name.c_str()), std::system_category()};
#else
	err = {mkdir(name.c_str(), 0777), std::system_category()}; // Create dir. if not already there.
#endif
    return err.value() == 0;
}

bool remove(
    const std::string& name,
    std::error_code& err
) noexcept
{
	err = {0, std::system_category()};
#if defined(_WIN32) && defined(UNICODE)
	const char *n = name.c_str();
	int nLen = std::strlen(n) + 1;
	LPTSTR lpszT = (LPTSTR) alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	if(!DeleteFile(lpszT))
		err = {GetLastError(), std::system_category()};
#else
		err = {std::remove(name.c_str()), std::system_category()};
#endif  /* (_WIN32) && defined(UNICODE) */
	return err.value() == 0;
}

void permissions(
    const std::string& file,
    perms prms,
    std::error_code& err
) noexcept
{
#ifdef _WIN32
//do nothing
	ignore_unused_variable_warning(file, prms, err);
#else /* !_WIN32 */
	err = {::chmod(file.c_str(), static_cast<mode_t>(prms)), std::system_category()};
#endif /* _WIN32 */
}


} /* namespace fs */
