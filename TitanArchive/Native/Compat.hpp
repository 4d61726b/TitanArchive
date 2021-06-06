#pragma once

#define DEFINE_GUID_CE(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) constexpr GUID name = {l, w1, w2, {b1, b2,  b3,  b4,  b5,  b6,  b7,  b8}}

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
typedef __int64 off64_t;
#define lseek64 _lseeki64
#define CompatDlopen(filename, flags)       LoadLibraryW(filename)
#define dlclose(handle)                     FreeLibrary(handle)
#define dlsym(handle, name)                 GetProcAddress(handle, name)
#define dlerror()                           to_wstring(::GetLastError())
typedef HMODULE                             CompatModule;
#define SLASH_CHAR                          L'\\'
#define close                               _close
#define lseek                               _lseek
#define dup                                 _dup
#else
#if defined(__GNUC__)
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#define CompatDlopen(filename, flags)       dlopen(conv(filename).c_str(), flags)
#define STDMETHODCALLTYPE
#define ULONG                               uint32_t
#define UNREFERENCED_PARAMETER(x)           (void)x
typedef void*                               CompatModule;
#include <cstdint>
#define interface struct
#define HRESULT int32_t
#define UINT uint32_t
#define S_OK (0)
#define S_FALSE (1)
#define E_FAIL	(0x80004005)
#define E_NOINTERFACE (0x80004002)
#define E_OUTOFMEMORY (0x80004003)
#define E_NOTIMPL     (0x80004001)
#define STG_E_INVALIDFUNCTION ((HRESULT)0x80030001L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define SLASH_STR                           L"/"
#define SLASH_CHAR                          L'/'

typedef struct _GUID {
    uint32_t  Data1;
    uint16_t  Data2;
    uint16_t  Data3;
    uint8_t   Data4[8];
} GUID;

#define REFIID const GUID&

// {00000000-0000-0000-C000-000000000046}
constexpr GUID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

interface IUnknown
{
    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual UINT AddRef() = 0;
    virtual UINT Release() = 0;
    virtual ~IUnknown() {}
};

typedef struct {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} FILETIME;

typedef uint16_t VARTYPE;
typedef uint32_t PROPID;

#define VT_BSTR		(8)

typedef wchar_t* BSTR;
typedef short VARIANT_BOOL;
typedef union _LARGE_INTEGER {
    struct {
        uint32_t LowPart;
        uint32_t HighPart;
    } DUMMYSTRUCTNAME;
    struct {
        uint32_t LowPart;
        uint32_t HighPart;
    } u;
    uint64_t QuadPart;
} LARGE_INTEGER;

typedef struct {
    VARTYPE           	vt;
    uint16_t    		wReserved1;
    uint16_t    		wReserved2;
    uint16_t    		wReserved3;
    union {
        char              cVal;
        uint8_t           bVal;
        int16_t           iVal;
        uint16_t          uiVal;
        int32_t           lVal;
        uint32_t          ulVal;
        float             fltVal;
        double            dblVal;
        char*             pcVal;
        BSTR              bstrVal;
        LARGE_INTEGER     uhVal;
        GUID*             puuid;
        FILETIME          filetime;
        VARIANT_BOOL      boolVal;
        /* ... */
    };
} PROPVARIANT;

#define VARIANT_TRUE ((VARIANT_BOOL)-1)
#define VARIANT_FALSE ((VARIANT_BOOL)0)

typedef enum tagSTREAM_SEEK
{
    STREAM_SEEK_SET = 0,
    STREAM_SEEK_CUR = 1,
    STREAM_SEEK_END = 2
} STREAM_SEEK;

uint32_t SysStringByteLen(const BSTR bstrElement);
BSTR SysAllocString(const wchar_t* wszStr);
void SysFreeString(BSTR bstrStr);
#else
#error "Unknown Platform"
#endif
#endif

int CompatOpenArchive(const wchar_t* wszFilename);
std::wstring conv(std::string from);
std::string conv(std::wstring from);

class CompatMmap
{
public:
    CompatMmap() {}
    
    CompatMmap& operator=(CompatMmap&& other)
    {
        if (this != &other)
        {
            Clear();
            m_iFd = other.m_iFd;
            m_pAddr = other.m_pAddr;
            m_szLength = other.m_szLength;
#if defined(_WIN32)
            m_hFileMapping = other.m_hFileMapping;
#endif
            other.Clear(false);
        }
        return *this;
    }

    void* Addr()
    {
#if defined(_WIN32)
        return m_pAddr ? m_pAddr : reinterpret_cast<void*>(-1);
#else
        return m_pAddr;
#endif
    }

    void Set(int iFd, size_t szLength)
    {
        Clear();

        m_iFd = dup(iFd);
        if (m_iFd == -1)
        {
            return;
        }

        if (lseek(m_iFd, 0, SEEK_SET) == -1)
        {
            Clear();
            return;
        }

#if defined(_WIN32)
        ULARGE_INTEGER uhVal;
        uhVal.QuadPart = szLength;
        m_hFileMapping = CreateFileMappingW(reinterpret_cast<HANDLE>(_get_osfhandle(m_iFd)), nullptr, PAGE_READONLY, uhVal.HighPart, uhVal.LowPart, nullptr);
        if (!m_hFileMapping)
        {
            Clear();
            return;
        }
        
        m_pAddr = MapViewOfFile(m_hFileMapping, FILE_MAP_READ, 0, 0, szLength);
        if (!m_pAddr)
        {
            Clear();
        }
#else
#if defined(__GNUC__)
        m_pAddr = mmap(0, szLength, PROT_READ, MAP_PRIVATE, m_iFd, 0);
        if (m_pAddr == reinterpret_cast<void*>(-1))
        {
            Clear();
        }
#else
#error "Unknown Platform"
#endif
#endif
    }

    void Clear(bool bFree = true)
    {
#if defined(_WIN32)
        if (m_pAddr && bFree)
        {
            UnmapViewOfFile(m_pAddr);
        }
        m_pAddr = nullptr;
        
        if (m_hFileMapping && bFree)
        {
            CloseHandle(m_hFileMapping);
        }
        m_hFileMapping = nullptr;
#else
#if defined(__GNUC__)
        if (m_pAddr != reinterpret_cast<void*>(-1) && bFree)
        {
            munmap(m_pAddr, m_szLength);
        }
        m_pAddr = reinterpret_cast<void*>(-1);        
#else
#error "Unknown Platform"
#endif
#endif
        if (m_iFd != -1 && bFree)
        {
            close(m_iFd);
        }
        m_iFd = -1;
        
        m_szLength = 0;
    }

    ~CompatMmap()
    {
        Clear();
    }

private:
    CompatMmap(const CompatMmap&) = delete;
    CompatMmap& operator=(const CompatMmap&) = delete;
    CompatMmap(CompatMmap&& other) = delete;

    int m_iFd = -1;
    void* m_pAddr = nullptr;
    size_t m_szLength = 0;
#if defined(_WIN32)
    HANDLE m_hFileMapping = nullptr;
#endif

};