#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include <string>

#include "TitanArchive.hpp"
#include "P7Zip.hpp"

using namespace std;

#if defined(_MSC_VER)
    #define EXPORT __declspec(dllexport)
    #define IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#else
#error "Unknown Platform"
#endif

HRESULT g_hrGlobalError = S_OK;
std::wstring g_wstrGlobalError;

void SetGlobalError(HRESULT hrError, const std::wstring& wstrError)
{
    g_hrGlobalError = hrError;
    g_wstrGlobalError = wstrError;
}

void SetGlobalError(HRESULT hrError, const std::string& strError)
{
    SetGlobalError(hrError, conv(strError));
}

static IArchiver* ArchiverFactory()
{
    try
    {
        return new C7ZipArchiver();
    }
    catch (...)
    {
        SetGlobalError(E_OUTOFMEMORY, "Out of memory creating C7ZipArchiver");
    }
    
    return nullptr;
}

extern "C"
{
    EXPORT ARCHIVER_STATUS GlobalInitialize(const wchar_t* wszLibPath);
    EXPORT ARCHIVER_STATUS GlobalAddCodec(const wchar_t* wszFormat, const wchar_t* wszLibPath);
    EXPORT void GlobalUninitialize();
    EXPORT void* CreateArchiveContext();
    EXPORT ARCHIVER_STATUS OpenArchiveMemory(void* pCtx, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat);
    EXPORT ARCHIVER_STATUS OpenArchiveDisk(void* pCtx, const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat);
#if defined(_WIN32)
    EXPORT ARCHIVER_STATUS OpenArchiveHandle(void* pCtx, HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat);
#else
    EXPORT ARCHIVER_STATUS OpenArchiveFD(void* pCtx, int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat);
#endif
    EXPORT ARCHIVER_STATUS GetArchiveFormat(void* pCtx, const wchar_t** pwszFormat);
    EXPORT ARCHIVER_STATUS GetArchiveItemCount(void* pCtx, uint32_t* pArchiveItemCount);
    EXPORT ARCHIVER_STATUS ListDirectory(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount);
    EXPORT ARCHIVER_STATUS GetArchiveItemPropertiesByPath(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItem);
    EXPORT ARCHIVER_STATUS GetArchiveItemPropertiesByIndex(void* pCtx, uint32_t ui32ItemIndex, ArchiveItem** ppItem);
    EXPORT ARCHIVER_STATUS FreeArchiveItem(void* pCtx, ArchiveItem* pItem);
    EXPORT ARCHIVER_STATUS ExtractArchiveItemToBufferByIndex(void* pCtx, uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword);
    EXPORT ARCHIVER_STATUS ExtractArchiveItemToBufferByPath(void* pCtx, const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword);
    EXPORT ARCHIVER_STATUS CloseArchive(void* pCtx);
    EXPORT ARCHIVER_STATUS DeleteArchiveContext(void* pCtx);
    EXPORT ARCHIVER_STATUS GetError(void* pCtx, HRESULT* pHr, const wchar_t** ppError);
}

void* CreateArchiveContext()
{
    return ArchiverFactory();
}

ARCHIVER_STATUS GlobalInitialize(const wchar_t* wszLibPath)
{
    return C7ZipArchiver::GlobalInitialize(wszLibPath);
}

ARCHIVER_STATUS GlobalAddCodec(const wchar_t* wszFormat, const wchar_t* wszLibPath)
{
    if (!wszFormat || !wszLibPath)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return C7ZipArchiver::GlobalAddCodec(wszFormat, wszLibPath);
}

void GlobalUninitialize()
{
    C7ZipArchiver::GlobalUninitialize();
}

ARCHIVER_STATUS OpenArchiveMemory(void* pCtx, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !pBuf || !ui64BufSize)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->OpenArchiveMemory(pBuf, ui64BufSize, wszPassword, wszFormat);
}

ARCHIVER_STATUS OpenArchiveDisk(void* pCtx, const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !wszPath)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->OpenArchiveDisk(wszPath, wszPassword, wszFormat);
}

#if defined(_WIN32)
ARCHIVER_STATUS OpenArchiveHandle(void* pCtx, HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->OpenArchiveHandle(hArchive, wszPassword, wszFormat); 
}
#else
ARCHIVER_STATUS OpenArchiveFD(void* pCtx, int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->OpenArchiveFD(iFd, wszPassword, wszFormat);
}
#endif

ARCHIVER_STATUS GetArchiveFormat(void* pCtx, const wchar_t** pwszFormat)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !pwszFormat)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->GetArchiveFormat(pwszFormat);
}

ARCHIVER_STATUS GetArchiveItemCount(void* pCtx, uint32_t* pArchiveItemCount)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !pArchiveItemCount)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->GetArchiveItemCount(pArchiveItemCount);
}

ARCHIVER_STATUS ListDirectory(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !wszPath || !ppItems)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->ListDirectory(wszPath, ppItems, pItemCount);     
}

ARCHIVER_STATUS GetArchiveItemPropertiesByPath(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItem)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !wszPath || !ppItem)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->GetArchiveItemProperties(wszPath, ppItem);
}

ARCHIVER_STATUS GetArchiveItemPropertiesByIndex(void* pCtx, uint32_t ui32ItemIndex, ArchiveItem** ppItem)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !ppItem)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->GetArchiveItemProperties(ui32ItemIndex, ppItem);
}

ARCHIVER_STATUS FreeArchiveItem(void* pCtx, ArchiveItem* pItem)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->FreeArchiveItem(pItem);
}

ARCHIVER_STATUS ExtractArchiveItemToBufferByIndex(void* pCtx, uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !pBuf || !ui64BufSize)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->ExtractArchiveItemToBuffer(ui32ItemIndex, pBuf, ui64BufSize, wszPassword);
}

ARCHIVER_STATUS ExtractArchiveItemToBufferByPath(void* pCtx, const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver || !wszPath || !pBuf || !ui64BufSize)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->ExtractArchiveItemToBuffer(wszPath, pBuf, ui64BufSize, wszPassword); 
}

ARCHIVER_STATUS CloseArchive(void* pCtx)
{
    IArchiver* pArchiver = static_cast<IArchiver*>(pCtx);
    if (!pArchiver)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    return pArchiver->CloseArchive();
}

ARCHIVER_STATUS DeleteArchiveContext(void* pCtx)
{
    delete static_cast<IArchiver*>(pCtx);
    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS GetError(void* pCtx, HRESULT* pHr, const wchar_t** ppError)
{
    if (pCtx)
    {
        return static_cast<IArchiver*>(pCtx)->GetError(pHr, ppError);
    }

    if (!pHr && !ppError)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    if (pHr)
    {
        *pHr = g_hrGlobalError;
    }

    if (ppError)
    {
        *ppError = g_wstrGlobalError.c_str();
    }

    return ARCHIVER_STATUS_SUCCESS;
}
