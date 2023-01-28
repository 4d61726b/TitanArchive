#pragma once

#include "Compat.hpp"

#define ARCHIVER_STATUS_SUCCESS    (0)
#define ARCHIVER_STATUS_FAILURE    (1)
#define ARCHIVER_STATUS            uint32_t

struct ArchiveItem
{
    uint32_t ui32Index;
    wchar_t* wszPath;
    char cIsDir;
    uint64_t ui64Size;
    FILETIME ftModTime;
    
    ArchiveItem* pItemNext;
};

interface IArchiver
{
    virtual ~IArchiver() {}
    virtual ARCHIVER_STATUS OpenArchiveMemory(uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat) = 0;
    virtual ARCHIVER_STATUS OpenArchiveDisk(const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat) = 0;
#if defined(_WIN32)
    virtual ARCHIVER_STATUS OpenArchiveHandle(HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat) = 0;
#else
    virtual ARCHIVER_STATUS OpenArchiveFD(int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat) = 0;
#endif
    virtual ARCHIVER_STATUS GetArchiveFormat(const wchar_t** pwszFormat) = 0;
    virtual ARCHIVER_STATUS GetArchiveItemCount(uint32_t* pArchiveItemCount) = 0;
    virtual ARCHIVER_STATUS ListDirectory(const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount) = 0;
    virtual ARCHIVER_STATUS GetArchiveItemProperties(const wchar_t* wszPath, ArchiveItem** ppItem) = 0;
    virtual ARCHIVER_STATUS GetArchiveItemProperties(uint32_t ui32ItemIndex, ArchiveItem** ppItem) = 0;
    virtual ARCHIVER_STATUS FreeArchiveItem(ArchiveItem* pItem) = 0;
    virtual ARCHIVER_STATUS ExtractArchiveItemToBuffer(uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword) = 0;
    virtual ARCHIVER_STATUS ExtractArchiveItemToBuffer(const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword) = 0;
    virtual ARCHIVER_STATUS CloseArchive() = 0;
    virtual ARCHIVER_STATUS GetError(HRESULT* pHr, const wchar_t** ppError) = 0;
};

void SetGlobalError(HRESULT hrError, const std::wstring& wstrError);
void SetGlobalError(HRESULT hrError, const std::string& strError);
