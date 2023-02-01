#include <cstdlib>
#include <cstring>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <mutex>

#include <fcntl.h>

#include "Compat.hpp"
#include "P7Zip.hpp"

using namespace std;

static CompatModule s_p7zModule = nullptr;
static bool s_bIsInitialized = false;

using fnCreateObject = HRESULT(*)(const GUID*, const GUID*, void**);
using fnGetNumberOfFormats = HRESULT(*)(uint32_t*);
using fnGetHandlerProperty2 = HRESULT(*)(uint32_t, PROPID, PROPVARIANT*);
static fnCreateObject s_CreateObject = nullptr;
static fnGetNumberOfFormats s_GetNumberOfFormats = nullptr;
static fnGetHandlerProperty2 s_GetHandlerProperty2 = nullptr;
static unordered_map<wstring /* Name */, ArchiveType> s_mapSupportedFormats;
static wstring s_wstrSupportedFormats;

// #define SINGLE_THREADED_ITERATION

#ifdef SINGLE_THREADED_ITERATION
#pragma message("Single threaded iteration is enabled")
#endif

#define INIT_CHECK()                                                    \
if (!s_bIsInitialized)                                                  \
{                                                                       \
    SetError(E_FAIL, "GlobalInitialize has not been called");           \
    return ARCHIVER_STATUS_FAILURE;                                     \
}
#define ARCHIVE_LOADED()                                                \
INIT_CHECK()                                                            \
if (!m_pInArchive)                                                      \
{                                                                       \
    SetError(E_FAIL, L"Archive has not been loaded");                   \
    return ARCHIVER_STATUS_FAILURE;                                     \
}

#define RESOLVE_FUNC_RET(mod, val, name)                                \
val = reinterpret_cast<decltype(val)>(dlsym(mod, name));                \
if (!val)                                                               \
{                                                                       \
    dlclose(mod);                                                       \
    mod = nullptr;                                                      \
    SetGlobalError(E_FAIL, L"Failed to resolve " name);                 \
    return ARCHIVER_STATUS_FAILURE;                                     \
}

C7ZipArchiver::C7ZipArchiver() {}

C7ZipArchiver::~C7ZipArchiver()
{
    CloseArchive();
}

ARCHIVER_STATUS C7ZipArchiver::OpenArchiveMemory(uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    INIT_CHECK();
    CloseArchive();
    
    HRESULT hr;
    IInArchive* pInArchive = nullptr;
    CBufInStream* pBufInStream = nullptr;
    CArchiveOpenCallback* pArchiveOpenCallback = nullptr;

    if (!wszFormat)
    {
        wszFormat = DiscoverArchiveFormat(pBuf, ui64BufSize);
        if (!wszFormat)
        {
            SetError(E_FAIL, L"Unable to discover archive format");
            return ARCHIVER_STATUS_FAILURE;
        }
    }

    pInArchive = CreateInArchive(wszFormat);
    if (!pInArchive)
    {
        return ARCHIVER_STATUS_FAILURE;
    }
    m_pInArchive = pInArchive;
    m_wstrArchiveFormat = wszFormat;

    try
    {
        pBufInStream = new CBufInStream(pBuf, ui64BufSize);
    }
    catch (...)
    {
        SetError(E_FAIL, L"Unable to create CBufInStream");
        return ARCHIVER_STATUS_FAILURE; 
    }
    pBufInStream->AddRef();

    if (wszPassword)
    {
        m_wstrPassword = wszPassword;
        
        try
        {
            pArchiveOpenCallback = new CArchiveOpenCallback(wszPassword);
        }
        catch (...)
        {
            SetError(E_FAIL, L"Unable to create CArchiveOpenCallback");
            return ARCHIVER_STATUS_FAILURE; 
        }
        pArchiveOpenCallback->AddRef();
    }

    hr = m_pInArchive->Open(pBufInStream, nullptr, pArchiveOpenCallback);

    if (pArchiveOpenCallback)
    {
        pArchiveOpenCallback->Release();
    }

    pBufInStream->Release();

    if (FAILED(hr))
    {
        SetError(hr, L"InArchive Open failed");
        return ARCHIVER_STATUS_FAILURE;
    }

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::OpenArchiveDisk(const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    INIT_CHECK();
    CloseArchive();

    int iFd;
    ARCHIVER_STATUS asStatus;
    
    iFd = CompatOpenArchive(wszPath);
    if (iFd == -1)
    {
        SetError(errno, L"Unable to open file");
        return ARCHIVER_STATUS_FAILURE;
    }

    asStatus = OpenArchiveFD(iFd, wszPassword, wszFormat);
    if (asStatus != ARCHIVER_STATUS_SUCCESS)
    {
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }

    m_iFd = iFd;

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::OpenArchiveFD(int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    INIT_CHECK();
    CloseArchive();

    CompatMmap cmMmap;
    off64_t off64Size;
    ARCHIVER_STATUS asStatus;

    iFd = dup(iFd);
    if (iFd == -1)
    {
        SetError(errno, L"Unable to duplicate handle");
        return ARCHIVER_STATUS_FAILURE;
    }

    off64Size = lseek64(iFd, 0, SEEK_END);
    if (off64Size == -1)
    {
        SetError(errno, L"Unable to determine file size");
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }

    if (!off64Size)
    {
        SetError(E_FAIL, L"Invalid archive size");
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }

    if (static_cast<uint64_t>(off64Size) > SIZE_MAX)
    {
        SetError(E_FAIL, L"Archive too large for memory");
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }

    cmMmap.Set(iFd, static_cast<size_t>(off64Size));
    if (cmMmap.Addr() == reinterpret_cast<void*>(-1))
    {
        SetError(errno, L"Unable to mmap file");
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }
    close(iFd);

    asStatus = OpenArchiveMemory(static_cast<uint8_t*>(cmMmap.Addr()), off64Size, wszPassword, wszFormat);
    if (asStatus != ARCHIVER_STATUS_SUCCESS)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    m_cmMmap = move(cmMmap);

    return ARCHIVER_STATUS_SUCCESS;
}

#if defined(_WIN32)
ARCHIVER_STATUS C7ZipArchiver::OpenArchiveHandle(HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat)
{
    HANDLE hDup = INVALID_HANDLE_VALUE;
    int iFd;

    if (DuplicateHandle(GetCurrentProcess(), hArchive, GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS) == FALSE)
    {
        SetError(E_FAIL, L"Unable to duplicate HANDLE");
        return ARCHIVER_STATUS_FAILURE;
    }
    
    iFd = _open_osfhandle(reinterpret_cast<intptr_t>(hDup), _O_RDONLY);
    if (iFd == -1)
    {
        SetError(E_FAIL, L"Unable to convert HANDLE to fd");
        CloseHandle(hDup);
        return ARCHIVER_STATUS_FAILURE;
    }
    
    if (OpenArchiveFD(iFd, wszPassword, wszFormat) != ARCHIVER_STATUS_SUCCESS)
    {
        close(iFd);
        return ARCHIVER_STATUS_FAILURE;
    }
    
    close(iFd);
    
    return ARCHIVER_STATUS_SUCCESS;
}
#endif

ARCHIVER_STATUS C7ZipArchiver::GetArchiveFormat(const wchar_t** pwszFormat)
{
    ARCHIVE_LOADED();

    *pwszFormat = m_wstrArchiveFormat.c_str();

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GetArchiveItemCount(uint32_t* pArchiveItemCount)
{
    ARCHIVE_LOADED();

    HRESULT hr;

    hr = m_pInArchive->GetNumberOfItems(pArchiveItemCount);
    if (FAILED(hr))
    {
        SetError(hr, "GetNumberOfItems failed");
        return ARCHIVER_STATUS_FAILURE;
    }

    return ARCHIVER_STATUS_SUCCESS;
}


ARCHIVER_STATUS C7ZipArchiver::ListDirectory(const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount)
{
    ARCHIVE_LOADED();
    
    unordered_map<wstring, ArchiveItem*> mapItems;
    volatile bool bHasDirectory = false;
    volatile bool bHasFailure = false;
    mutex mLock;

    *ppItems = nullptr;
    
    if (pItemCount)
    {
        *pItemCount = 0;
    }

    if (IterateItems([=, &mLock, &bHasFailure, &bHasDirectory, &mapItems](uint32_t ui32Start, uint32_t ui32End)
    {
        C7ZipProperty c7zPropPath(m_pInArchive);
        unordered_set<wstring> setDiscoveredDirectories;
        unordered_map<wstring, ArchiveItem*> mapLocalItems;
        
        for (uint32_t i = ui32Start; i < ui32End && !bHasFailure; ++i)
        {
            wchar_t* pSlash = nullptr;
            
            if (FAILED(c7zPropPath.GetProperty(i, kpidPath)))
            {
                continue;
            }

            if (c7zPropPath->bstrVal)
            {
                pSlash = wcsrchr(c7zPropPath->bstrVal, SLASH_CHAR);
            }

            if (!pSlash)
            {
                if (*wszPath == L'\0')
                {
                    ArchiveItem* pItem;
                    if (GetArchiveItemProperties(i, &pItem) == ARCHIVER_STATUS_FAILURE)
                    {
                        bHasFailure = true;
                        break;
                    }
                    else
                    {
                        mapLocalItems[pItem->wszPath] = pItem;
                    }
                }
            }
            else
            {
                *pSlash = L'\0';
                if (wcscmp(wszPath, c7zPropPath->bstrVal) == 0)
                {
                    *pSlash = SLASH_CHAR;
                    if (*(pSlash + 1) != L'\0' && !wcschr(pSlash + 1, SLASH_CHAR))
                    {
                        ArchiveItem* pItem;
                        if (GetArchiveItemProperties(i, &pItem) == ARCHIVER_STATUS_FAILURE)
                        {
                            bHasFailure = true;
                            break;
                        }
                        else
                        {
                            mapLocalItems[pItem->wszPath] = pItem;
                        }
                    }
                }
                else
                {
                    if (!wcschr(c7zPropPath->bstrVal, SLASH_CHAR) && *wszPath == L'\0')
                    {
                        setDiscoveredDirectories.insert(c7zPropPath->bstrVal);
                    }
                    
                    *pSlash = SLASH_CHAR;
                }
            }
            
            if (!bHasDirectory && wcswcs(c7zPropPath->bstrVal, wszPath) == c7zPropPath->bstrVal)
            {
                bHasDirectory = true;
            }
            
        }
        
        mLock.lock();
        for (const auto& iterItem : mapLocalItems)
        {
            auto iterItem2 = mapItems.find(iterItem.first);
            if (iterItem2 != mapItems.end())
            {
                FreeArchiveItem(iterItem2->second);
            }
            mapItems[iterItem.first] = iterItem.second;
        }
        
        for (const wstring& wstrDirectory : setDiscoveredDirectories)
        {
            if (mapItems.find(wstrDirectory) == mapItems.end())
            {
                ArchiveItem* pItem = CreateDirectoryPlaceholder(wstrDirectory.c_str());
                if (!pItem)
                {
                    bHasFailure = true;
                    break;
                }
                else
                {
                    mapItems[wstrDirectory] = pItem;
                }
            }
        }
        mLock.unlock();
        
    }) == ARCHIVER_STATUS_FAILURE)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    for (auto iterItemNext = mapItems.begin(); iterItemNext != mapItems.end();)
    {
        auto iterItem = iterItemNext++;
        wchar_t* wszOldPath = iterItem->second->wszPath;
        wchar_t* wszBaseName;
        if (iterItemNext != mapItems.end())
        {
            iterItem->second->pItemNext = iterItemNext->second;
        }
        wszBaseName = wcsrchr(wszOldPath, SLASH_CHAR);
        wszBaseName = wszBaseName ? wszBaseName + 1 : wszOldPath;
        if (wszBaseName != wszOldPath)
        {
            iterItem->second->wszPath = SysAllocString(wszBaseName);
            SysFreeString(wszOldPath);
        }
    }
    
    if (bHasFailure || !bHasDirectory)
    {
        if (mapItems.size() > 0)
        {
            FreeArchiveItem(mapItems.begin()->second);
        }
        return ARCHIVER_STATUS_FAILURE;
    }

    if (mapItems.size() > 0)
    {
        *ppItems = mapItems.begin()->second;
    }

    if (pItemCount)
    {
        *pItemCount = mapItems.size();
    }

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GetArchiveItemProperties(const wchar_t* wszPath, ArchiveItem** ppItem)
{
    ARCHIVE_LOADED();

    volatile uint32_t ui32FoundItem = UINT_MAX;
    volatile bool bHasDirectory = false;
    
    if (IterateItems([=, &bHasDirectory, &ui32FoundItem](uint32_t ui32Start, uint32_t ui32End)
    {
        C7ZipProperty c7zPropPath(m_pInArchive);
        
        for (uint32_t i = ui32Start; i < ui32End && ui32FoundItem == UINT_MAX; ++i)
        {
            if (FAILED(c7zPropPath.GetProperty(i, kpidPath)))
            {
                continue;
            }
            
            if (c7zPropPath->bstrVal)
            {
                if (wcscmp(wszPath, c7zPropPath->bstrVal) == 0)
                {
                    ui32FoundItem = i;
                }
                else if (!bHasDirectory && wcswcs(c7zPropPath->bstrVal, wszPath) == c7zPropPath->bstrVal)
                {
                    bHasDirectory = true;
                }
            }
            else if (wszPath[0] == L'\0')
            {
                ui32FoundItem = i;
            }
        }
    }) == ARCHIVER_STATUS_FAILURE)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    if (ui32FoundItem == UINT_MAX)
    {
        if (!bHasDirectory || wszPath[0] == L'\0')
        {
            SetError(E_FAIL, L"Path not found");
            return ARCHIVER_STATUS_FAILURE;
        }
        
        *ppItem = CreateDirectoryPlaceholder(wszPath);
        if (!*ppItem)
        {
            return ARCHIVER_STATUS_FAILURE;
        }
        
        return ARCHIVER_STATUS_SUCCESS;
    }

    return GetArchiveItemProperties(ui32FoundItem, ppItem);
}

ARCHIVER_STATUS C7ZipArchiver::GetArchiveItemProperties(uint32_t ui32ItemIndex, ArchiveItem** ppItem)
{
    ARCHIVE_LOADED();

    C7ZipProperty c7zPropPath(m_pInArchive);
    C7ZipProperty c7zPropIsDir(m_pInArchive);
    C7ZipProperty c7zPropSize(m_pInArchive);
    C7ZipProperty c7zPropMTime(m_pInArchive);
    uint32_t ui32ItemCount;
    HRESULT hr;

    hr = m_pInArchive->GetNumberOfItems(&ui32ItemCount);
    if (FAILED(hr))
    {
        SetError(hr, "GetNumberOfItems failed");
        return ARCHIVER_STATUS_FAILURE;
    }

    if (ui32ItemIndex >= ui32ItemCount)
    {
        SetError(E_FAIL, "Invalid item index");
        return ARCHIVER_STATUS_FAILURE;
    }

    if (FAILED(c7zPropPath.GetProperty(ui32ItemIndex, kpidPath)))
    {
        c7zPropPath->bstrVal = nullptr;
    }

    hr = c7zPropIsDir.GetProperty(ui32ItemIndex, kpidIsDir);
    if (FAILED(hr))
    {
        SetError(hr, L"GetProperty failed (kpidIsDir)");
        return ARCHIVER_STATUS_FAILURE;
    }

    hr = c7zPropSize.GetProperty(ui32ItemIndex, kpidSize);
    if (FAILED(hr))
    {
        SetError(hr, L"GetProperty failed (kpidSize)");
        return ARCHIVER_STATUS_FAILURE;
    }

    hr = c7zPropMTime.GetProperty(ui32ItemIndex, kpidMTime);
    if (FAILED(hr))
    {
        SetError(hr, L"GetProperty failed (kpidMTime)");
        return ARCHIVER_STATUS_FAILURE;
    }
    
    *ppItem = static_cast<ArchiveItem*>(calloc(1, sizeof(ArchiveItem)));
    if (!*ppItem)
    {
        SetError(E_OUTOFMEMORY, L"Unable to create ArchiveItem");
        return ARCHIVER_STATUS_FAILURE;
    }
    
    (*ppItem)->ui32Index = ui32ItemIndex;
    (*ppItem)->wszPath = c7zPropPath.Release().bstrVal;
    if (!(*ppItem)->wszPath)
    {
        wchar_t* wszEmptyPath = SysAllocString(L"");
        if (!wszEmptyPath)
        {
            SetError(E_OUTOFMEMORY, L"Unable to create empty path string");
            free(*ppItem);
            return ARCHIVER_STATUS_FAILURE;
        }
        (*ppItem)->wszPath = wszEmptyPath;
    }
    (*ppItem)->cIsDir = c7zPropIsDir->boolVal == VARIANT_TRUE;
    (*ppItem)->ui64Size = c7zPropSize->uhVal.QuadPart;
    (*ppItem)->ftModTime = c7zPropMTime->filetime;

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::FreeArchiveItem(ArchiveItem* pItem)
{   
    while (pItem)
    {
        ArchiveItem* pItemNext = pItem->pItemNext;
        SysFreeString(pItem->wszPath);
        free(pItem);
        pItem = pItemNext;
    }

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::ExtractArchiveItemToBuffer(uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
{
    ARCHIVE_LOADED();

    HRESULT hr;
    CArchiveExtractCallback* pArchiveExtractCallback;
    IArchiveExtractCallback* pArchiveExtractCallbackInterface;

    if (!wszPassword && !m_wstrPassword.empty())
    {
        wszPassword = m_wstrPassword.c_str();
    }
    
    try
    {        
        pArchiveExtractCallback = new CArchiveExtractCallback(pBuf, ui64BufSize, wszPassword);
    }
    catch (...)
    {
        SetError(E_OUTOFMEMORY, L"Out of memory creating CArchiveExtractCallback");
        return ARCHIVER_STATUS_FAILURE;
    }

    hr = pArchiveExtractCallback->QueryInterface(IID_IArchiveExtractCallback, reinterpret_cast<void**>(&pArchiveExtractCallbackInterface));
    if (FAILED(hr))
    {
        SetError(hr, L"QueryInterface failed on CArchiveExtractCallback");
        return ARCHIVER_STATUS_FAILURE;
    }

    hr = m_pInArchive->Extract(&ui32ItemIndex, 1, 0, pArchiveExtractCallbackInterface);
    pArchiveExtractCallback->Release();

    if (FAILED(hr))
    {
        SetError(hr, L"InArchive Extract failed");
        return ARCHIVER_STATUS_FAILURE;
    }

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::ExtractArchiveItemToBuffer(const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
{
    ARCHIVE_LOADED();

    ArchiveItem* aiElement;
    uint32_t ui32ItemIndex;

    if (GetArchiveItemProperties(wszPath, &aiElement) != ARCHIVER_STATUS_SUCCESS)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    ui32ItemIndex = aiElement->ui32Index;

    FreeArchiveItem(aiElement);

    return ExtractArchiveItemToBuffer(ui32ItemIndex, pBuf, ui64BufSize, wszPassword);
}

ARCHIVER_STATUS C7ZipArchiver::CloseArchive()
{
    INIT_CHECK();

    if (m_pInArchive)
    {
        m_pInArchive->Release();
        m_pInArchive = nullptr;
    }

    m_wstrArchiveFormat.clear();

    m_cmMmap.Clear();

    if (m_iFd != -1)
    {
        close(m_iFd);
        m_iFd = -1;
    }

    m_wstrPassword.clear();

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GetError(HRESULT* pHr, const wchar_t** ppError)
{
    if (!pHr && !ppError)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    if (pHr)
    {
        *pHr = m_hrError;
    }

    if (ppError)
    {
        *ppError = m_wstrError.c_str();
    }

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GlobalInitialize(const wchar_t* wszLibPath)
{
    if (!wszLibPath)
    {
        SetGlobalError(E_FAIL, "7z library path missing");
        return ARCHIVER_STATUS_FAILURE;
    }

    if (s_bIsInitialized)
    {
        GlobalUninitialize();
    }

    s_p7zModule = CompatDlopen(wszLibPath, RTLD_LOCAL | RTLD_NOW);
    if (!s_p7zModule)
    {
        SetGlobalError(E_FAIL, dlerror());
        return ARCHIVER_STATUS_FAILURE;
    }

    RESOLVE_FUNC_RET(s_p7zModule, s_GetNumberOfFormats, "GetNumberOfFormats");
    RESOLVE_FUNC_RET(s_p7zModule, s_GetHandlerProperty2, "GetHandlerProperty2");
    RESOLVE_FUNC_RET(s_p7zModule, s_CreateObject, "CreateObject");

    if (PopulateArchiveSupport() != ARCHIVER_STATUS_SUCCESS)
    {
        return ARCHIVER_STATUS_FAILURE;
    }

    s_bIsInitialized = true;
    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GlobalAddCodec(const wchar_t* wszFormat, const wchar_t* wszLibPath)
{
    struct ArchiveType::CodecModule cmElem;

    if (!s_bIsInitialized)
    {
        SetGlobalError(E_FAIL, "GlobalInitialize has not been called");
        return ARCHIVER_STATUS_FAILURE;
    }
    
    auto iterElem = s_mapSupportedFormats.find(wszFormat);

    if (iterElem == s_mapSupportedFormats.end())
    {
        SetGlobalError(E_FAIL, "Format is not supported");
        return ARCHIVER_STATUS_FAILURE;
    }

    cmElem.pModule = CompatDlopen(wszLibPath, RTLD_LOCAL | RTLD_NOW);
    if (!cmElem.pModule)
    {
        SetGlobalError(E_FAIL, dlerror());
        return ARCHIVER_STATUS_FAILURE;
    }

    RESOLVE_FUNC_RET(cmElem.pModule, cmElem.pGetNumberOfMethods, "GetNumberOfMethods");
    RESOLVE_FUNC_RET(cmElem.pModule, cmElem.pCreateDecoder, "CreateDecoder");
    RESOLVE_FUNC_RET(cmElem.pModule, cmElem.pGetMethodProperty, "GetMethodProperty");

    if (iterElem->second.cmCodec.pModule)
    {
        dlclose(iterElem->second.cmCodec.pModule);
    }

    iterElem->second.cmCodec = cmElem;

    return ARCHIVER_STATUS_SUCCESS;
}

ARCHIVER_STATUS C7ZipArchiver::GlobalGetSupportedArchiveFormats(const wchar_t** ppArchiveFormats)
{
    if (!s_bIsInitialized)
    {
        SetGlobalError(E_FAIL, "GlobalInitialize has not been called");
        return ARCHIVER_STATUS_FAILURE;
    }

    *ppArchiveFormats = s_wstrSupportedFormats.c_str();

    return ARCHIVER_STATUS_SUCCESS;
}

void C7ZipArchiver::GlobalUninitialize()
{
    for (auto& iterSupportedType : s_mapSupportedFormats)
    {
        if (iterSupportedType.second.cmCodec.pModule)
        {
            dlclose(iterSupportedType.second.cmCodec.pModule);
        }
    }

    s_mapSupportedFormats.clear();
    s_wstrSupportedFormats.clear();

    if (s_p7zModule)
    {
        dlclose(s_p7zModule);
        s_p7zModule = nullptr;
    }

    s_GetNumberOfFormats = nullptr;
    s_GetHandlerProperty2 = nullptr;
    s_CreateObject = nullptr;

    s_bIsInitialized = false;
}

ARCHIVER_STATUS C7ZipArchiver::IterateItems(function<void(uint32_t /* Start index */, uint32_t /* End index */)> const& f)
{
    vector<thread> vecWorkers;
    uint32_t ui32ThreadCount = thread::hardware_concurrency();
#ifndef SINGLE_THREADED_ITERATION
    constexpr uint32_t ui32ThreadThreshold = 500;
#endif
    uint32_t ui32ItemCount = 0;
    HRESULT hr;
    
    if (!ui32ThreadCount)
    {
        SetError(E_FAIL, L"Unable to determine core count");
        return ARCHIVER_STATUS_FAILURE;
    }

    hr = m_pInArchive->GetNumberOfItems(&ui32ItemCount);
    if (FAILED(hr))
    {
        SetError(hr, "GetNumberOfItems failed");
        return ARCHIVER_STATUS_FAILURE;
    }

#ifndef SINGLE_THREADED_ITERATION
    if (ui32ItemCount < ui32ThreadThreshold)
    {
#endif
        ui32ThreadCount = 1;
#ifndef SINGLE_THREADED_ITERATION
    }
#endif

    for (uint32_t ui32Thread = 0; ui32Thread < ui32ThreadCount; ++ui32Thread)
    {
        uint32_t ui32Start = (ui32ItemCount / ui32ThreadCount) * ui32Thread;
        uint32_t ui32End = (ui32ItemCount / ui32ThreadCount) * (ui32Thread + 1);
        
        if (ui32End > ui32ItemCount || ui32Thread == ui32ThreadCount - 1)
        {
            ui32End = ui32ItemCount;
        }
#ifndef SINGLE_THREADED_ITERATION
        vecWorkers.push_back(thread([=]()
        {
            f(ui32Start, ui32End);
        }));
#else
        f(ui32Start, ui32End);
#endif
    }

#ifndef SINGLE_THREADED_ITERATION
    for (thread& threadElem : vecWorkers)
    {
        threadElem.join();
    }
#endif

    return ARCHIVER_STATUS_SUCCESS;
}

ArchiveItem* C7ZipArchiver::CreateDirectoryPlaceholder(const wchar_t* wszDirectoryPath)
{
    ArchiveItem* pItem = static_cast<ArchiveItem*>(calloc(1, sizeof(ArchiveItem)));
    if (!pItem)
    {
        SetError(E_OUTOFMEMORY, L"Unable to create ArchiveItem");
        return nullptr;
    }
    
    pItem->ui32Index = UINT_MAX;
    pItem->wszPath = SysAllocString(wszDirectoryPath);
    pItem->cIsDir = 1;
    
    return pItem;
}

C7ZipArchiver::IInArchive* C7ZipArchiver::CreateInArchive(const wchar_t* wszFormat)
{
    IInArchive* pInArchive = nullptr;
    HRESULT hr;

    if (!wszFormat)
    {
        SetError(E_FAIL, "Archive format argument missing");
        return nullptr;
    }

    auto iterElem = s_mapSupportedFormats.find(wszFormat);
    if (iterElem == s_mapSupportedFormats.end())
    {
        SetError(E_FAIL, L"\"" + wstring(wszFormat) + L"\" is not a supported format");
        return nullptr;
    }

    hr = s_CreateObject(&iterElem->second.guidClassId, &IID_IInArchive, reinterpret_cast<void**>(&pInArchive));
    if (FAILED(hr))
    {
        SetError(hr, "Unable to create IInArchive object");
        return nullptr;
    }
    
    if (iterElem->second.cmCodec.pModule)
    {
        ISetCompressCodecsInfo* pSetCompressCodecsInfo = nullptr;

        if (SUCCEEDED(pInArchive->QueryInterface(IID_ISetCompressCodecsInfo, reinterpret_cast<void**>(&pSetCompressCodecsInfo))))
        {
            CCompressCodecsInfo* pCompressCodecsInfo;
            try
            {
                pCompressCodecsInfo = new CCompressCodecsInfo(&iterElem->second.cmCodec);
            }
            catch(...)
            {
                SetError(E_FAIL, L"Unable to create CCompressCodecsInfo");
                return nullptr; 
            }
            
            pCompressCodecsInfo->AddRef();
            pSetCompressCodecsInfo->SetCompressCodecsInfo(pCompressCodecsInfo);
            pSetCompressCodecsInfo->Release();
        }
    }

    return pInArchive;
}

const wchar_t* C7ZipArchiver::DiscoverArchiveFormat(uint8_t* pBuf, uint64_t ui64BufSize)
{
    for (const auto& itElem : s_mapSupportedFormats)
    {
        for (const auto& vecSig : itElem.second.vecSignatures)
        {
            if (itElem.second.ui32SignatureOffset + vecSig.size() > ui64BufSize)
            {
                continue;
            }
            if (memcmp(vecSig.data(), pBuf + itElem.second.ui32SignatureOffset, vecSig.size()) == 0)
            {
                return itElem.first.c_str();                 
            }
        }
    }

    return nullptr;
}

ARCHIVER_STATUS C7ZipArchiver::PopulateArchiveSupport()
{
    uint32_t ui32FormatCount;
    HRESULT hr;

    enum
    {
        kName = 0,        // VT_BSTR
        kClassID,         // binary GUID in VT_BSTR
        kExtension,       // VT_BSTR
        kAddExtension,    // VT_BSTR
        kUpdate,          // VT_BOOL
        kKeepName,        // VT_BOOL
        kSignature,       // binary in VT_BSTR
        kMultiSignature,  // binary in VT_BSTR
        kSignatureOffset, // VT_UI4
        kAltStreams,      // VT_BOOL
        kNtSecure,        // VT_BOOL
        kFlags            // VT_UI4
    };

    hr = s_GetNumberOfFormats(&ui32FormatCount);
    if (FAILED(hr))
    {
        SetGlobalError(hr, "GetNumberOfFormats failed");
        return ARCHIVER_STATUS_FAILURE;
    }
    if (ui32FormatCount == 0)
    {
        SetGlobalError(E_FAIL, "No supported formats");
        return ARCHIVER_STATUS_FAILURE;
    }

    for (uint32_t i = 0; i < ui32FormatCount; ++i)
    {
        C7ZipProperty c7zProp(s_GetHandlerProperty2);
        ArchiveType iaElement = {0};
        wstring wstrName;

        hr = c7zProp.GetProperty(i, kName);
        if (FAILED(hr))
        {
            SetGlobalError(hr, "GetHandlerProperty2 failed (kName)");
            return ARCHIVER_STATUS_FAILURE;
        }
        wstrName = c7zProp->bstrVal;

        hr = c7zProp.GetProperty(i, kClassID);
        if (FAILED(hr))
        {
            SetGlobalError(hr, L"GetHandlerProperty2 failed (kClassID) (" + wstrName + L")");
            return ARCHIVER_STATUS_FAILURE;
        }
        iaElement.guidClassId = *reinterpret_cast<GUID*>(c7zProp->bstrVal);

        hr = c7zProp.GetProperty(i, kSignature);
        if (FAILED(hr) || c7zProp->bstrVal == nullptr || SysStringByteLen(c7zProp->bstrVal) == 0)
        {
            hr = c7zProp.GetProperty(i, kMultiSignature);
            if (SUCCEEDED(hr) && c7zProp->bstrVal != nullptr && SysStringByteLen(c7zProp->bstrVal) != 0)
            {
                uint32_t ui32TotalSignatureLen = SysStringByteLen(c7zProp->bstrVal);
                uint8_t* ui8Signature = reinterpret_cast<uint8_t*>(c7zProp->bstrVal);
                while (ui32TotalSignatureLen != 0)
                {
                    vector<uint8_t> vecSignature;
                    uint8_t ui8CurrentSignatureLength = *ui8Signature++;
                    if (ui8CurrentSignatureLength == 0xff || static_cast<uint32_t>(ui8CurrentSignatureLength + 1) > ui32TotalSignatureLen)
                    {
                        SetGlobalError(E_FAIL, L"Malformed signature detected (" + wstrName + L")");
                        return ARCHIVER_STATUS_FAILURE;
                    }
                    vecSignature.resize(ui8CurrentSignatureLength);
                    memcpy(vecSignature.data(), ui8Signature, vecSignature.size());
                    iaElement.vecSignatures.push_back(vecSignature);
                    ui8Signature += ui8CurrentSignatureLength;
                    ui32TotalSignatureLen -= (ui8CurrentSignatureLength + 1);
                }
            }
        }
        else
        {
            vector<uint8_t> vecSignature;
            vecSignature.resize(SysStringByteLen(c7zProp->bstrVal));
            memcpy(vecSignature.data(), c7zProp->bstrVal, vecSignature.size());
            iaElement.vecSignatures.push_back(vecSignature);
        }

        hr = c7zProp.GetProperty(i, kSignatureOffset);
        if (FAILED(hr))
        {
            SetGlobalError(hr, L"GetHandlerProperty2 failed (kSignatureOffset) (" + wstrName + L")");
            return ARCHIVER_STATUS_FAILURE;
        }
        iaElement.ui32SignatureOffset = c7zProp->ulVal;

        s_mapSupportedFormats[wstrName] = iaElement;

        if (!s_wstrSupportedFormats.empty())
        {
            s_wstrSupportedFormats += L",";
        }
        s_wstrSupportedFormats += wstrName;
    }

    return ARCHIVER_STATUS_SUCCESS;
}

void C7ZipArchiver::SetError(HRESULT hrError, const wstring& wstrError)
{
    m_hrError = hrError;
    m_wstrError = wstrError;
}

void C7ZipArchiver::SetError(HRESULT hrError, const string& strError)
{
    SetError(hrError, conv(strError));
}

