#if _WIN32
#	include <dson/os/windows.h>
#elif __APPLE__
#	include <dson/os/apple.h>
#elif __ANDROID__
#	include <dson/os/android.h>
#elif __linux__
#	include <dson/os/linux.h>
#endif
