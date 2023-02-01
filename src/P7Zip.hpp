#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <climits>

#include "TitanArchive.hpp"

// {23170F69-40C1-278A-0000-000600600000}
DEFINE_GUID_CE(IID_IInArchive, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x06, 0x00, 0x60, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000300030000}
DEFINE_GUID_CE(IID_IInStream, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000000050000"}
DEFINE_GUID_CE(IID_IProgress, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000600200000} 
DEFINE_GUID_CE(IID_IArchiveExtractCallback, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x06, 0x00, 0x20, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000300010000}
DEFINE_GUID_CE(IID_ISequentialInStream, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000400610000}
DEFINE_GUID_CE(IID_ISetCompressCodecsInfo, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x61, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000400600000}
DEFINE_GUID_CE(IID_ICompressCodecsInfo, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x60, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000600100000} 
DEFINE_GUID_CE(IID_IArchiveOpenCallback, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x06, 0x00, 0x10, 0x00, 0x00);

// {23170F69-40C1-278A-0000-000500100000} 
DEFINE_GUID_CE(IID_ICryptoGetTextPassword, 0x23170F69, 0x40C1, 0x278A, 0x00, 0x00, 0x00, 0x05, 0x00, 0x10, 0x00, 0x00);

struct ArchiveType
{
    GUID guidClassId;
    std::vector<std::vector<uint8_t>> vecSignatures;
    uint32_t ui32SignatureOffset;
    struct CodecModule
    {
        using fnGetNumberOfMethods = HRESULT(*)(uint32_t *numMethods);
        using fnGetMethodProperty = HRESULT(*)(uint32_t index, PROPID propID, PROPVARIANT *value);
        using fnCreateDecoder = HRESULT(*)(uint32_t index, const GUID *iid, void **coder);
        CompatModule pModule = nullptr;
        fnGetNumberOfMethods pGetNumberOfMethods;
        fnCreateDecoder pCreateDecoder;
        fnGetMethodProperty pGetMethodProperty;
    } cmCodec;
};

class C7ZipArchiver : public IArchiver
{
public:
    C7ZipArchiver();
    virtual ~C7ZipArchiver();

    ARCHIVER_STATUS OpenArchiveMemory(uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat) override;
    ARCHIVER_STATUS OpenArchiveDisk(const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat) override;
#if defined(_WIN32)
    ARCHIVER_STATUS OpenArchiveHandle(HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat) override;
#else
    ARCHIVER_STATUS OpenArchiveFD(int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat) override;   
#endif
    ARCHIVER_STATUS GetArchiveFormat(const wchar_t** pwszFormat) override;
    ARCHIVER_STATUS GetArchiveItemCount(uint32_t* pArchiveItemCount) override;
    ARCHIVER_STATUS ListDirectory(const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount) override;
    ARCHIVER_STATUS GetArchiveItemProperties(const wchar_t* wszPath, ArchiveItem** ppItem) override;
    ARCHIVER_STATUS GetArchiveItemProperties(uint32_t ui32ItemIndex, ArchiveItem** ppItem) override;
    ARCHIVER_STATUS FreeArchiveItem(ArchiveItem* pItem) override;
    ARCHIVER_STATUS ExtractArchiveItemToBuffer(uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword) override;
    ARCHIVER_STATUS ExtractArchiveItemToBuffer(const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword) override;
    ARCHIVER_STATUS CloseArchive() override;
    ARCHIVER_STATUS GetError(HRESULT* pHr, const wchar_t** ppError) override;
    static ARCHIVER_STATUS GlobalInitialize(const wchar_t* wszLibPath);
    static ARCHIVER_STATUS GlobalAddCodec(const wchar_t* wszFormat, const wchar_t* wszLibPath);
    static ARCHIVER_STATUS GlobalGetSupportedArchiveFormats(const wchar_t** pwszFormats);
    static void GlobalUninitialize();

private:

#if defined(_WIN32)
    ARCHIVER_STATUS OpenArchiveFD(int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat);
#endif

    enum
    {
        kpidPath = 3,
        kpidIsDir = 6,
        kpidSize = 7,
        kpidCTime = 10,
        kpidATime = 11,
        kpidMTime = 12,
        kpidTimeType = 40
    };

    interface ICryptoGetTextPassword : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE CryptoGetTextPassword(BSTR *password) = 0;
    };

    interface ICompressCodecsInfo : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE GetNumMethods(uint32_t *numMethods) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetProperty(uint32_t index, PROPID propID, PROPVARIANT *value) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateDecoder(uint32_t index, const GUID *iid, void **coder) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateEncoder(uint32_t index, const GUID *iid, void **coder) = 0;
    };

    interface ISetCompressCodecsInfo : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE SetCompressCodecsInfo(ICompressCodecsInfo *compressCodecsInfo) = 0;
    };

    interface IInStream : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE Read(uint8_t *data, uint32_t size, uint32_t *processedSize) = 0;
        virtual HRESULT STDMETHODCALLTYPE Seek(int64_t offset, uint32_t seekOrigin, uint64_t *newPosition) = 0;
    };

    interface IProgress : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE SetTotal(uint64_t total) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetCompleted(const uint64_t *completeValue) = 0;
    };

    interface ISequentialInStream : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE Read(void *data, uint32_t size, uint32_t *processedSize) = 0;
    };

    interface IArchiveExtractCallback : public IProgress
    {
        virtual HRESULT STDMETHODCALLTYPE GetStream(uint32_t index, ISequentialInStream **inStream) = 0;
        virtual HRESULT STDMETHODCALLTYPE PrepareOperation(int32_t askExtractMode) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetOperationResult(int32_t operationResult) = 0;
    };

    interface IArchiveOpenCallback : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE SetTotal(const uint64_t *files, const uint64_t *bytes) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetCompleted(const uint64_t *files, const uint64_t *bytes) = 0;
    };

    interface IInArchive : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE Open(IInStream *stream, const uint64_t *maxCheckStartPosition, IArchiveOpenCallback *openArchiveCallback) = 0;
        virtual HRESULT STDMETHODCALLTYPE Close() = 0;
        virtual HRESULT STDMETHODCALLTYPE GetNumberOfItems(uint32_t *numItems) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetProperty(uint32_t index, PROPID propID, PROPVARIANT *value) = 0;
        virtual HRESULT STDMETHODCALLTYPE Extract(const uint32_t *indices, uint32_t numItems, uint32_t testMode, IArchiveExtractCallback *extractCallback) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetArchiveProperty(PROPID propID, PROPVARIANT *value) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetNumberOfProperties(uint32_t *numProperties) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo(uint32_t index, wchar_t **name, PROPID *propID, VARTYPE *varType) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetNumberOfArchiveProperties(uint32_t *numProperties) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(uint32_t index, wchar_t **name, PROPID *propID, VARTYPE *varType) = 0;
    };

    struct CCompressCodecsInfo : public ICompressCodecsInfo
    {
    public:
        CCompressCodecsInfo(const ArchiveType::CodecModule* pCodec) : m_uiRefCount(0), m_pCodec(pCodec) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (memcmp(&riid, &IID_IUnknown, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IUnknown*>(static_cast<ICompressCodecsInfo*>(this));
            }
            else if (memcmp(&riid, &IID_ICompressCodecsInfo, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<ICompressCodecsInfo*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }
            AddRef();          
            return S_OK;
        }
    
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return m_uiRefCount.fetch_add(1) + 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG uiRef = m_uiRefCount.fetch_sub(1) - 1;
            if (uiRef == 0)
            {
                delete this;
            }
            return uiRef;
        }

        HRESULT STDMETHODCALLTYPE GetNumMethods(uint32_t *numMethods) override
        {
            return m_pCodec->pGetNumberOfMethods(numMethods);
        }

        HRESULT STDMETHODCALLTYPE GetProperty(uint32_t index, PROPID propID, PROPVARIANT *value) override
        {
            return m_pCodec->pGetMethodProperty(index, propID, value);
        }

        HRESULT STDMETHODCALLTYPE CreateDecoder(uint32_t index, const GUID *iid, void **coder) override
        {
            return m_pCodec->pCreateDecoder(index, iid, coder);
        }

        HRESULT STDMETHODCALLTYPE CreateEncoder(uint32_t index, const GUID *iid, void **coder) override
        {
            UNREFERENCED_PARAMETER(index);
            UNREFERENCED_PARAMETER(iid);
            UNREFERENCED_PARAMETER(coder);
            return E_NOTIMPL;
        }

    private:
        virtual ~CCompressCodecsInfo() {}
        std::atomic_uint m_uiRefCount;
        const ArchiveType::CodecModule* m_pCodec;
    };

    struct CSequentialInStream : public ISequentialInStream
    {
        CSequentialInStream(uint8_t* pBuf, uint64_t ui64BufSize) : m_uiRefCount(0), m_pBuf(pBuf), m_ui64BufSize(ui64BufSize) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (memcmp(&riid, &IID_IUnknown, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IUnknown*>(static_cast<ISequentialInStream*>(this));
            }
            else if (memcmp(&riid, &IID_ISequentialInStream, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<ISequentialInStream*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }
            AddRef();          
            return S_OK;
        }
    
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return m_uiRefCount.fetch_add(1) + 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG uiRef = m_uiRefCount.fetch_sub(1) - 1;
            if (uiRef == 0)
            {
                delete this;
            }
            return uiRef;
        }

        HRESULT STDMETHODCALLTYPE Read(void *data, uint32_t size, uint32_t *processedSize) override
        {
            if (processedSize)
            {
                *processedSize = size;
            }
            if (size == 0)
            {
                return S_OK;
            }
            if (m_ui64BufPos >= m_ui64BufSize)
            {
                return S_OK;
            }
            uint64_t ui64Rem = m_ui64BufSize - m_ui64BufPos;
            if (ui64Rem > size)
            {
                ui64Rem = size;
            }
            memcpy(m_pBuf + m_ui64BufPos, data, static_cast<size_t>(ui64Rem));
            m_ui64BufPos += ui64Rem;

            return S_OK;
        }

    private:
        virtual ~CSequentialInStream() {}

        std::atomic_uint m_uiRefCount;
        uint8_t* m_pBuf = nullptr;
        uint64_t m_ui64BufSize = 0;
        uint64_t m_ui64BufPos = 0;
    };

    struct CArchiveOpenCallback : public IArchiveOpenCallback, public ICryptoGetTextPassword
    {
    public:
        CArchiveOpenCallback(const wchar_t* wszPassword) : m_uiRefCount(0)
        {
            if (wszPassword)
            {
                m_wstrPassword = wszPassword;
            }
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (memcmp(&riid, &IID_IUnknown, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IUnknown*>(static_cast<IArchiveOpenCallback*>(this));
            }
            else if (memcmp(&riid, &IID_IArchiveOpenCallback, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IArchiveOpenCallback*>(this);
            }
            else if (memcmp(&riid, &IID_ICryptoGetTextPassword, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<ICryptoGetTextPassword*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }
            AddRef();          
            return S_OK;
        }
    
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return m_uiRefCount.fetch_add(1) + 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG uiRef = m_uiRefCount.fetch_sub(1) - 1;
            if (uiRef == 0)
            {
                delete this;
            }
            return uiRef;
        }

        HRESULT STDMETHODCALLTYPE SetTotal(const uint64_t *files, const uint64_t *bytes) override
        {
            UNREFERENCED_PARAMETER(files);
            UNREFERENCED_PARAMETER(bytes);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE SetCompleted(const uint64_t *files, const uint64_t *bytes) override
        {
            UNREFERENCED_PARAMETER(files);
            UNREFERENCED_PARAMETER(bytes);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CryptoGetTextPassword(BSTR *password) override
        {
            if (m_wstrPassword.empty())
            {
                return E_FAIL;
            }

            *password = SysAllocString(m_wstrPassword.c_str());
            if (!*password)
            {
                return E_OUTOFMEMORY;
            }

            return S_OK;
        }

    private:
        virtual ~CArchiveOpenCallback() {}

        std::atomic_uint m_uiRefCount;
        std::wstring m_wstrPassword;

    };

    struct CArchiveExtractCallback : public IArchiveExtractCallback, public ICryptoGetTextPassword
    {
        CArchiveExtractCallback(uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword) : m_uiRefCount(0), m_pBuf(pBuf), m_ui64BufSize(ui64BufSize)
        {
            if (wszPassword)
            {
                m_wstrPassword = wszPassword;
            }
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (memcmp(&riid, &IID_IUnknown, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IUnknown*>(static_cast<IArchiveExtractCallback*>(this));
            }
            else if (memcmp(&riid, &IID_IArchiveExtractCallback, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IArchiveExtractCallback*>(this);
            }
            else if (memcmp(&riid, &IID_IProgress, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IProgress*>(this);
            }
            else if (memcmp(&riid, &IID_ICryptoGetTextPassword, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<ICryptoGetTextPassword*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }
            AddRef();          
            return S_OK;
        }
    
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return m_uiRefCount.fetch_add(1) + 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG uiRef = m_uiRefCount.fetch_sub(1) - 1;
            if (uiRef == 0)
            {
                delete this;
            }
            return uiRef;
        }
        
        HRESULT STDMETHODCALLTYPE GetStream(uint32_t index, ISequentialInStream **inStream) override
        {
            UNREFERENCED_PARAMETER(index);
            
            try
            {
                m_pSequentialInStream = new CSequentialInStream(m_pBuf, m_ui64BufSize);
            }
            catch (...)
            {
                return E_OUTOFMEMORY;
            }

            m_pSequentialInStream->AddRef();

            return m_pSequentialInStream->QueryInterface(IID_ISequentialInStream, reinterpret_cast<void**>(inStream));
        }
        HRESULT STDMETHODCALLTYPE PrepareOperation(int32_t askExtractMode) override
        {
            UNREFERENCED_PARAMETER(askExtractMode);
            
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetOperationResult(int32_t operationResult) override
        {
            if (operationResult)
            {
                return E_FAIL;
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetTotal(uint64_t total) override
        {
            UNREFERENCED_PARAMETER(total);
            
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetCompleted(const uint64_t *completeValue) override
        {
            UNREFERENCED_PARAMETER(completeValue);
            
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CryptoGetTextPassword(BSTR *password) override
        {
            if (m_wstrPassword.empty())
            {
                return E_FAIL;
            }

            *password = SysAllocString(m_wstrPassword.c_str());
            if (!*password)
            {
                return E_OUTOFMEMORY;
            }

            return S_OK;
        }

    private:
        virtual ~CArchiveExtractCallback()
        {
            if (m_pSequentialInStream)
            {
                m_pSequentialInStream->Release();
                m_pSequentialInStream = nullptr;
            }
        }

        std::atomic_uint m_uiRefCount;
        uint8_t* m_pBuf = nullptr;
        uint64_t m_ui64BufSize = 0;
        std::wstring m_wstrPassword;
        CSequentialInStream* m_pSequentialInStream = nullptr;
    };

    class C7ZipProperty
    {
    public:
        using fn7ZipGetProperty = HRESULT(*)(uint32_t, PROPID, PROPVARIANT*);

        C7ZipProperty(fn7ZipGetProperty fnGetProperty) : m_GetProperty(fnGetProperty) {}
        C7ZipProperty(IInArchive* pInArchive) : m_pInArchive(pInArchive) {}
        ~C7ZipProperty()
        {
            if (m_bNeedsFree)
            {
                PropVariantFree(m_pvElement);
                m_pvElement = {0};
                m_bNeedsFree = false;
            }
        }

        HRESULT GetProperty(uint32_t ui32Index, PROPID propId)
        {
            HRESULT hr;
            
            if (m_bNeedsFree)
            {
                PropVariantFree(m_pvElement);
                m_pvElement = {0};
                m_bNeedsFree = false;
            }

            if (m_GetProperty)
            {
                hr = m_GetProperty(ui32Index, propId, &m_pvElement);
            }
            else
            {
                hr = m_pInArchive->GetProperty(ui32Index, propId, &m_pvElement);
            }

            if (FAILED(hr))
            {
                return hr;
            }

            m_bNeedsFree = true;

            return hr;
        }
        
        PROPVARIANT* operator->()
        {
            return &m_pvElement;
        }

        PROPVARIANT Release()
        {
            PROPVARIANT pvRet = m_pvElement;
            m_pvElement = {0};
            m_bNeedsFree = false;
            return pvRet;
        }

    private:

        fn7ZipGetProperty m_GetProperty = nullptr;
        IInArchive* m_pInArchive = nullptr;

        PROPVARIANT m_pvElement = {0};
        bool m_bNeedsFree = false;
    };

    struct CBufInStream : public IInStream
    {
    public:
        CBufInStream(uint8_t* pBuf, uint64_t ui64BufSize) : m_pBuf(pBuf), m_ui64BufSize(ui64BufSize), m_uiRefCount(0) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (memcmp(&riid, &IID_IUnknown, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IUnknown*>(static_cast<IInStream*>(this));
            }
            else if (memcmp(&riid, &IID_IInStream, sizeof(GUID)) == 0)
            {
                *ppvObject = static_cast<IInStream*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }
    
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return m_uiRefCount.fetch_add(1) + 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG uiRef = m_uiRefCount.fetch_sub(1) - 1;
            if (uiRef == 0)
            {
                delete this;
            }
            return uiRef;
        }

        HRESULT STDMETHODCALLTYPE Read(uint8_t *data, uint32_t size, uint32_t *processedSize) override
        {
            if (processedSize)
            {
                *processedSize = 0;
            }
            if (size == 0)
            {
                return S_OK;
            }
            if (m_ui64BufPos >= m_ui64BufSize)
            {
                return S_OK;
            }
            uint64_t ui64Rem = m_ui64BufSize - m_ui64BufPos;
            if (ui64Rem > size)
            {
                ui64Rem = size;
            }
            memcpy(data, m_pBuf + m_ui64BufPos, static_cast<size_t>(ui64Rem));
            m_ui64BufPos += ui64Rem;
            if (processedSize)
            {
                *processedSize = static_cast<uint32_t>(ui64Rem);
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Seek(int64_t offset, uint32_t seekOrigin, uint64_t *newPosition) override
        {
            switch (seekOrigin)
            {
                case STREAM_SEEK_SET:
                    break;
                case STREAM_SEEK_CUR:
                    offset += m_ui64BufPos;
                    break;
                case STREAM_SEEK_END:
                    offset += m_ui64BufSize;
                    break;
                default:
                    return STG_E_INVALIDFUNCTION;
            }
            if (offset < 0)
            {
                constexpr HRESULT E_NEGATIVE_SEEK = 0x80070083;
                return E_NEGATIVE_SEEK;
            }
            m_ui64BufPos = offset;
            if (newPosition)
            {
                *newPosition = offset;
            }
            return S_OK;
        }
    private:
        virtual ~CBufInStream() {}

        const uint8_t* m_pBuf = nullptr;
        uint64_t m_ui64BufSize = 0;
        uint64_t m_ui64BufPos = 0;
        std::atomic_uint m_uiRefCount;
    };

    static void PropVariantFree(PROPVARIANT pvElement)
    {
        if (pvElement.vt == VT_BSTR && pvElement.bstrVal != nullptr)
        {
            SysFreeString(pvElement.bstrVal);
        }
    }

    ARCHIVER_STATUS IterateItems(std::function<void(uint32_t /* Start index */, uint32_t /* End index */)> const& f);
    ArchiveItem* CreateDirectoryPlaceholder(const wchar_t* wszDirectoryPath);
    IInArchive* CreateInArchive(const wchar_t* wszFormat);
    const wchar_t* DiscoverArchiveFormat(uint8_t* pBuf, uint64_t ui64BufSize);

    void SetError(HRESULT hrError, const std::wstring& wstrError);
    void SetError(HRESULT hrError, const std::string& strError);

    static ARCHIVER_STATUS PopulateArchiveSupport();

    IInArchive* m_pInArchive = nullptr;
    std::wstring m_wstrArchiveFormat;
    
    int m_iFd = -1;

    CompatMmap m_cmMmap;

    std::wstring m_wstrPassword;

    HRESULT m_hrError = S_OK;
    std::wstring m_wstrError;
};
