#include "Util.h"

#include <windows.h>

long long Util::nowMillis()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return static_cast<long long>(value.QuadPart / 10000ULL - 11644473600000LL);
}
