import ctypes
from io import BytesIO
from collections import namedtuple
import os
import sys
if os.name == 'nt':
    from ctypes import wintypes
    import msvcrt

ARCHIVER_STATUS_SUCCESS = 0
ARCHIVER_STATUS_FAILURE = 1
E_FAIL = 0x80004005

if os.name == 'nt':
    ext = '.dll'
elif os.name == 'posix':
    ext = '.so'
else:
    raise Exception('Unknown platform')
if ctypes.sizeof(ctypes.c_voidp) == 4:
    bits = '32'
elif ctypes.sizeof(ctypes.c_voidp) == 8:
    bits = '64'
else:
    raise Exception('Unknown architecture')

TITAN_ARCHIVE_MODULE_NAME = 'TitanArchive{}{}'.format(bits, ext)
TITAN_ARCHIVE_7Z_MODULE_NAME = 'TitanArchive{}-7z{}'.format(bits, ext)

if sys.prefix == sys.base_prefix:
    if os.name == 'nt':
        TITAN_ARCHIVE_MODULE_DIR = os.path.join(sys.prefix, 'DLLs')
    else:
        TITAN_ARCHIVE_MODULE_DIR = os.path.join('/', 'usr', 'local', 'lib')
    TITAN_ARCHIVE_MODULE = os.path.join(TITAN_ARCHIVE_MODULE_DIR, TITAN_ARCHIVE_MODULE_NAME)
else:
    if os.name == 'nt':
        mod_dir = 'DLLs'
    else:
        mod_dir = 'lib'

    TITAN_ARCHIVE_MODULE_DIR = os.path.join(sys.prefix, mod_dir)
    TITAN_ARCHIVE_MODULE = os.path.join(TITAN_ARCHIVE_MODULE_DIR, TITAN_ARCHIVE_MODULE_NAME)
    if not os.path.isfile(TITAN_ARCHIVE_MODULE):
        TITAN_ARCHIVE_MODULE_DIR = os.path.join(sys.base_prefix, mod_dir)
        TITAN_ARCHIVE_MODULE = os.path.join(TITAN_ARCHIVE_MODULE_DIR, TITAN_ARCHIVE_MODULE_NAME)

if not os.path.isfile(TITAN_ARCHIVE_MODULE):
    raise Exception('Unable to find TitanArchive support module (%s)' % TITAN_ARCHIVE_MODULE_NAME)

TITAN_ARCHIVE_7Z_MODULE = os.path.join(TITAN_ARCHIVE_MODULE_DIR, TITAN_ARCHIVE_7Z_MODULE_NAME)

if not os.path.isfile(TITAN_ARCHIVE_7Z_MODULE):
    raise Exception('Unable to find TitanArchive 7z support module (%s)' % TITAN_ARCHIVE_7Z_MODULE)

class TitanArchiveException(Exception):
    def __init__(self, hr, err_string):
        self.hr = hr
        self.err_string = err_string
        super().__init__(self.err_string)

class _ArchiveItem(ctypes.Structure):
    _fields_ = [('Index', ctypes.c_uint),
                ('Path', ctypes.c_wchar_p),
                ('IsDir', ctypes.c_bool),
                ('Size', ctypes.c_ulonglong),
                ('MTime', ctypes.c_ulonglong),
                ('_ItemNext', ctypes.c_void_p)]

def _GetDict(st):
    d = dict((field, getattr(st, field)) for field, _ in st._fields_ if not field.startswith('_'))
    ArchiveItem = namedtuple('ArchiveItem', d)
    return ArchiveItem(**d)

def _GetNamedTuple(struct):
    rtn = collections.namedtuple()

class TitanArchive():

    def __del__(self):
        self._DeleteArchiveContext()

    def __init__(self, archive, password = None, archive_format = None):
        self.archive = archive
        self.password = password
        self.archive_format = archive_format
        self._ctx = ctypes.c_void_p(lib.CreateArchiveContext())
        
        if self._ctx.value == ctypes.c_void_p(0).value:
            raise TitanArchiveException(*GetGlobalError())
        if isinstance(self.archive, int):
            self.OpenArchiveFD(self.archive, self.password, self.archive_format)
        elif isinstance(self.archive, str):
            self.OpenArchiveDisk(self.archive, self.password, self.archive_format)
        else:
            self.OpenArchiveMemory(self.archive, self.password, self.archive_format)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self._DeleteArchiveContext()

    def __iter__(self):
        for i in range(0, self.GetArchiveItemCount()):
            yield self.GetArchiveItemPropertiesByIndex(i)

    def OpenArchiveMemory(self, buf, password = None, archive_format = None):
        size = len(buf)
        if lib.OpenArchiveMemory(self._ctx, buf, ctypes.c_ulonglong(size), ctypes.c_wchar_p(password), ctypes.c_wchar_p(archive_format)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())

    def OpenArchiveDisk(self, path, password = None, archive_format = None):
        if lib.OpenArchiveDisk(self._ctx, ctypes.c_wchar_p(path), ctypes.c_wchar_p(password), ctypes.c_wchar_p(archive_format)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())

    def OpenArchiveFD(self, fd, password = None, archive_format = None):
        if os.name == 'nt':
            if lib.OpenArchiveHandle(self._ctx, wintypes.HANDLE(msvcrt.get_osfhandle(fd)), ctypes.c_wchar_p(password), ctypes.c_wchar_p(archive_format)) != ARCHIVER_STATUS_SUCCESS:
                raise TitanArchiveException(*self.GetError())
        else:
            if lib.OpenArchiveFD(self._ctx, ctypes.c_int(fd), ctypes.c_wchar_p(password), ctypes.c_wchar_p(archive_format)) != ARCHIVER_STATUS_SUCCESS:
                raise TitanArchiveException(*self.GetError())

    def GetArchiveFormat(self):
        rtn = ctypes.c_wchar_p()
        if lib.GetArchiveFormat(self._ctx, ctypes.byref(rtn)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        return rtn.value

    def GetArchiveItemCount(self):
        rtn = ctypes.c_uint()
        if lib.GetArchiveItemCount(self._ctx, ctypes.byref(rtn)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        return rtn.value

    def ListDirectory(self, path = ''):
        rtn = []
        ai = ctypes.POINTER(_ArchiveItem)()
        count = ctypes.c_ulonglong()
        if lib.ListDirectory(self._ctx, ctypes.c_wchar_p(path), ctypes.byref(ai), ctypes.byref(count)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        cur = ai
        for i in range(0, count.value):
            cur = cur.contents
            rtn.append(_GetDict(cur))
            cur = ctypes.cast(cur._ItemNext, ctypes.POINTER(_ArchiveItem))
        self._FreeArchiveItem(ai)
        return rtn

    def GetArchiveItemPropertiesByPath(self, path):
        ai = ctypes.POINTER(_ArchiveItem)()
        if lib.GetArchiveItemPropertiesByPath(self._ctx, ctypes.c_wchar_p(path), ctypes.byref(ai)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        ai = ai.contents
        rtn = _GetDict(ai)
        self._FreeArchiveItem(ai)
        return rtn

    def GetArchiveItemPropertiesByIndex(self, index):
        ai = ctypes.POINTER(_ArchiveItem)()
        if lib.GetArchiveItemPropertiesByIndex(self._ctx, ctypes.c_uint(index), ctypes.byref(ai)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        ai = ai.contents
        rtn = _GetDict(ai)
        self._FreeArchiveItem(ai)
        return rtn

    def ExtractArchiveItemToBufferByIndex(self, index, password = None):
        size = self.GetArchiveItemPropertiesByIndex(index).Size
        rtn = BytesIO(bytearray(size))
        if size and lib.ExtractArchiveItemToBufferByIndex(self._ctx, ctypes.c_uint(index), rtn.getvalue(), ctypes.c_ulonglong(size), ctypes.c_wchar_p(password)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        return rtn

    def ExtractArchiveItemToBufferByPath(self, path, password = None):
        size = self.GetArchiveItemPropertiesByPath(path).Size
        rtn = BytesIO(bytearray(size))
        if size and lib.ExtractArchiveItemToBufferByPath(self._ctx, ctypes.c_wchar_p(path), ctypes.c_char_p(rtn.getvalue()), ctypes.c_ulonglong(size), ctypes.c_wchar_p(password)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())
        return rtn

    def CloseArchive(self):
        if lib.CloseArchive(self._ctx) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())

    def _FreeArchiveItem(self, ai):
        if lib.FreeArchiveItem(self._ctx, ai) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(*self.GetError())

    def _DeleteArchiveContext(self):
        if self._ctx.value != ctypes.c_void_p(0).value:
            if lib.DeleteArchiveContext(self._ctx) != ARCHIVER_STATUS_SUCCESS:
                raise TitanArchiveException(*self.GetError())
            else:
                self._ctx = ctypes.c_void_p(0)

    def GetError(self):
        hr = ctypes.c_int()
        err = ctypes.c_wchar_p()
        if lib.GetError(self._ctx, ctypes.byref(hr), ctypes.byref(err)) != ARCHIVER_STATUS_SUCCESS:
            raise TitanArchiveException(E_FAIL, 'Unable to get error')
        return (hr.value, err.value)

def GetGlobalError():
    hr = ctypes.c_int()
    err = ctypes.c_wchar_p()
    if lib.GetError(None, ctypes.byref(hr), ctypes.byref(err)) != ARCHIVER_STATUS_SUCCESS:
        raise TitanArchiveException(E_FAIL, 'Unable to get error')
    return (hr.value, err.value)

def GlobalInitialize(lib_path):
    if lib.GlobalInitialize(lib_path) != ARCHIVER_STATUS_SUCCESS:
        raise TitanArchiveException(*GetGlobalError())

def GlobalAddCodec(format, lib_path):
    if lib.GlobalAddCodec(format, lib_path) != ARCHIVER_STATUS_SUCCESS:
        raise TitanArchiveException(*GetGlobalError())
        
def GlobalUninitialize():
    lib.GlobalUninitialize()

lib = ctypes.CDLL(TITAN_ARCHIVE_MODULE)

# ARCHIVER_STATUS GlobalInitialize(const wchar_t* wszLibPath)
lib.GlobalInitialize.argtypes = [ctypes.c_wchar_p]
lib.GlobalInitialize.restype = ctypes.c_uint

# ARCHIVER_STATUS GlobalAddCodec(const wchar_t* wszFormat, const wchar_t* wszLibPath);
lib.GlobalAddCodec.argtype = [ctypes.c_wchar_p, ctypes.c_wchar_p]
lib.GlobalAddCodec.restype = ctypes.c_uint

# void GlobalUninitialize()
lib.GlobalUninitialize.argtypes = []

# void* CreateArchiveContext()
lib.CreateArchiveContext.restype = ctypes.c_void_p

# ARCHIVER_STATUS OpenArchiveMemory(void* pCtx, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword, const wchar_t* wszFormat)
lib.OpenArchiveMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_wchar_p, ctypes.c_wchar_p]
lib.OpenArchiveMemory.restype = ctypes.c_uint

# ARCHIVER_STATUS OpenArchiveDisk(void* pCtx, const wchar_t* wszPath, const wchar_t* wszPassword, const wchar_t* wszFormat)
lib.OpenArchiveDisk.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
lib.OpenArchiveDisk.restype = ctypes.c_uint

if os.name == 'nt':
    # ARCHIVER_STATUS OpenArchiveHandle(void* pCtx, HANDLE hArchive, const wchar_t* wszPassword, const wchar_t* wszFormat)
    lib.OpenArchiveHandle.argtypes = [ctypes.c_void_p, wintypes.HANDLE, ctypes.c_wchar_p, ctypes.c_wchar_p]
    lib.OpenArchiveHandle.restype = ctypes.c_uint
else:
    # ARCHIVER_STATUS OpenArchiveFD(void* pCtx, int iFd, const wchar_t* wszPassword, const wchar_t* wszFormat)
    lib.OpenArchiveFD.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_wchar_p, ctypes.c_wchar_p]
    lib.OpenArchiveFD.restype = ctypes.c_uint

# ARCHIVER_STATUS GetArchiveFormat(void* pCtx, const wchar_t** pwszFormat)
lib.GetArchiveFormat.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_wchar_p)]
lib.GetArchiveFormat.restype = ctypes.c_uint

# ARCHIVER_STATUS GetArchiveItemCount(void* pCtx, uint32_t* pArchiveItemCount)
lib.GetArchiveItemCount.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint)]
lib.GetArchiveItemCount.restype = ctypes.c_uint

# ARCHIVER_STATUS ListDirectory(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItems, uint64_t* pItemCount)
lib.ListDirectory.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.POINTER(_ArchiveItem)), ctypes.POINTER(ctypes.c_ulonglong)]
lib.ListDirectory.restype = ctypes.c_uint

# ARCHIVER_STATUS GetArchiveItemPropertiesByPath(void* pCtx, const wchar_t* wszPath, ArchiveItem** ppItem)
lib.GetArchiveItemPropertiesByPath.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.POINTER(_ArchiveItem))]
lib.GetArchiveItemPropertiesByPath.restype = ctypes.c_uint

# ARCHIVER_STATUS GetArchiveItemPropertiesByIndex(void* pCtx, uint32_t ui32ItemIndex, ArchiveItem** ppItem)
lib.GetArchiveItemPropertiesByIndex.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.POINTER(_ArchiveItem))]
lib.GetArchiveItemPropertiesByIndex.restype = ctypes.c_uint

# ARCHIVER_STATUS FreeArchiveItem(void* pCtx, ArchiveItem* pItem)
lib.FreeArchiveItem.argtypes = [ctypes.c_void_p, ctypes.POINTER(_ArchiveItem)]
lib.FreeArchiveItem.restype = ctypes.c_uint

# ARCHIVER_STATUS ExtractArchiveItemToBufferByIndex(void* pCtx, uint32_t ui32ItemIndex, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
lib.ExtractArchiveItemToBufferByIndex.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_wchar_p]
lib.ExtractArchiveItemToBufferByIndex.restype = ctypes.c_uint

# ARCHIVER_STATUS ExtractArchiveItemToBufferByPath(void* pCtx, const wchar_t* wszPath, uint8_t* pBuf, uint64_t ui64BufSize, const wchar_t* wszPassword)
lib.ExtractArchiveItemToBufferByPath.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_wchar_p]
lib.ExtractArchiveItemToBufferByPath.restype = ctypes.c_uint

# ARCHIVER_STATUS CloseArchive(void* pCtx)
lib.CloseArchive.argtypes = [ctypes.c_void_p]
lib.CloseArchive.restype = ctypes.c_uint

# ARCHIVER_STATUS DeleteArchiveContext(void* pCtx)
lib.DeleteArchiveContext.argtypes = [ctypes.c_void_p]
lib.DeleteArchiveContext.restype = ctypes.c_uint

# ARCHIVER_STATUS GetError(void* pCtx, HRESULT* pHr, const wchar_t** ppError)
lib.GetError.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_wchar_p)]
lib.GetError.restype = ctypes.c_uint

GlobalInitialize(TITAN_ARCHIVE_7Z_MODULE)
