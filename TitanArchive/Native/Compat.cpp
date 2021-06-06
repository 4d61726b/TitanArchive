#include <string>

#include <cwchar>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>

#if defined(_WIN32)
#include <sys\stat.h>
#endif

#include "Compat.hpp"

using namespace std;

wstring conv(string from)
{
    if (from.size() == 0)
    {
        return L"";
    }
    
    wstring to(from.size() + 1, L'\0');
#if defined(_WIN32)
    size_t szRet;
    mbstowcs_s(&szRet, const_cast<wchar_t*>(to.data()), to.size(), from.c_str(), to.size());
#else
    mbstowcs(const_cast<wchar_t*>(to.data()), from.c_str(), to.size());
#endif
    return to;
}
string conv(wstring from)
{
    if (from.size() == 0)
    {
        return "";
    }
    
    string to(from.size() + 1, L'\0');
#if defined(_WIN32)
    size_t szRet;
    wcstombs_s(&szRet, const_cast<char*>(to.data()), to.size(), from.c_str(), to.size());
#else
    wcstombs(const_cast<char*>(to.data()), from.c_str(), to.size());
#endif
    return to;
}

#if !defined(_WIN32)
uint32_t SysStringByteLen(const BSTR bstrElement)
{
    return *(reinterpret_cast<uint32_t*>(bstrElement) - 1);
}

BSTR SysAllocString(const wchar_t* wszStr)
{
    wchar_t* wszRet;
    size_t szCount;

    if (!wszStr)
    {
        return nullptr;
    }

    szCount = wcslen(wszStr);

    wszRet = static_cast<wchar_t*>(malloc((szCount + 1) * sizeof(wchar_t) + sizeof(uint32_t)));
    if (!wszRet)
    {
        return nullptr;
    }

    *(reinterpret_cast<uint32_t*>(wszRet)) = szCount;
    wszRet = reinterpret_cast<wchar_t*>(reinterpret_cast<uint32_t*>(wszRet) + 1);
    wcscpy(wszRet, wszStr);

    return wszRet;
}

void SysFreeString(BSTR bstrStr)
{
    free(reinterpret_cast<uint32_t*>(bstrStr) - 1);
}

int CompatOpenArchive(const wchar_t* wszFilename)
{
    return open(conv(wszFilename).c_str(), O_RDONLY);
}
#endif

#if defined(_WIN32)
int CompatOpenArchive(const wchar_t* wszFilename)
{
    errno_t errRet;
    int iFd = -1;
    
    errRet = _wsopen_s(&iFd, wszFilename, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IREAD);
    if (iFd == -1)
    {
        errno = errRet;
    }
    return iFd;
}
#endif