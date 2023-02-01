# TitanArchive
An easy to use multiplatform archive extraction wrapper library for [7-Zip](https://www.7-zip.org/)

## Features

* Easy API for C and Python. Greatly simplifies the complexity of using [7-Zip](https://www.7-zip.org/)
* Multiplatform. Supports Linux and Windows
* Very fast. Written in modern C++
* Automatically discovers and determines archive type
* Supports most archive formats: 7z, XZ, BZIP2, GZIP, TAR, ZIP, WIM, AR, ARJ, CAB, CHM, CPIO, CramFS, DMG, EXT, FAT, GPT, HFS, IHEX, ISO, LZH, LZMA, MBR, MSI, NSIS, NTFS, QCOW2, RAR, RPM, SquashFS, UDF, UEFI, VDI, VHD, VMDK, WIM, XAR and Z

## Usage

### Python

#### Open archive on disk:
```python
from titanarchive import TitanArchive

with TitanArchive('test.zip') as ta:
    # Do actions
```

#### Open archive from memory buffer:
```python
from titanarchive import TitanArchive

data = open('test.zip', 'rb').read()

with TitanArchive(data) as ta:
    # Do actions
```

#### Open archive from file descriptor:
```python
import os
from titanarchive import TitanArchive

fd = os.open('test.zip', os.O_RDONLY | (os.O_BINARY if os.name == 'nt' else 0))

with TitanArchive(fd) as ta:
    # Do actions
```

#### Show supported archive formats:
```python
import titanarchive

print(titanarchive.GlobalGetSupportedArchiveFormats())
```
```console
['APFS', 'APM', 'Ar', 'Arj', 'Base64', 'bzip2', 'Compound', 'Cpio', 'CramFS', 'Dmg', 'ELF', 'Ext', 'FAT', 'FLV', 'gzip', 'GPT', 'HFS', 'IHex', 'LP', 'Lzh', 'lzma', 'lzma86', 'MachO', 'MBR', 'MsLZ', 'Mub', 'NTFS', 'PE', 'COFF', 'TE', 'Ppmd', 'QCOW', 'Rpm', 'Sparse', 'Split', 'SquashFS', 'SWFc', 'SWF', 'UEFIc', 'UEFIf', 'VDI', 'VHD', 'VHDX', 'VMDK', 'Xar', 'xz', 'Z', '7z', 'Cab', 'Chm', 'Hxs', 'Iso', 'Nsis', 'Rar', 'Rar5', 'tar', 'Udf', 'wim', 'zip']
```

#### Discover archive format:
```python
from titanarchive import TitanArchive

with TitanArchive('unknown.dat') as ta:
    print('Format: {}'.format(ta.GetArchiveFormat()))
```
```console
Format: zip
```

#### Print all files and directories in archive (Method 1, by path):
```python
from titanarchive import TitanArchive
import os

def print_files(ta, path):
    for item in ta.ListDirectory(path):
        full_path = os.path.join(path, item.Path)
        if item.IsDir:
            print_files(ta, full_path)
        print('Item: {}'.format(full_path))

with TitanArchive('test.zip') as ta:
    print_files(ta, '')
```
```console
Item: dir1\another_file.txt
Item: dir1\dir2\another_file.txt
Item: dir1\dir2\dir3\file.txt
Item: dir1\dir2\dir3
Item: dir1\dir2\empty_directory
Item: dir1\dir2\file.txt
Item: dir1\dir2
Item: dir1\empty_directory
Item: dir1
Item: empty_directory
Item: file_at_root.txt
```

#### Print all files and directories in archive (Method 2, by index):
```python
from titanarchive import TitanArchive

with TitanArchive('test.zip') as ta:
    try:
        index = 0
        while True:
            item = ta.GetArchiveItemPropertiesByIndex(index)
            print('Item: {}'.format(item.Path))
            index += 1
    except titanarchive.TitanArchiveException:
        pass
```
```console
Item: dir1
Item: dir1\another_file.txt
Item: dir1\dir2
Item: dir1\dir2\another_file.txt
Item: dir1\dir2\dir3
Item: dir1\dir2\dir3\file.txt
Item: dir1\dir2\empty_directory
Item: dir1\dir2\file.txt
Item: dir1\empty_directory
Item: empty_directory
Item: file_at_root.txt
```

#### Extract file to memory (Method 1, by path):
```python
from titanarchive import TitanArchive

with TitanArchive('test.zip') as ta:
    data = ta.ExtractArchiveItemToBufferByPath('dir1\\another_file.txt')
    print(data.read())
```
```console
b'Test Data 123'
```

#### Extract file to memory (Method 2, by index):
```python
from titanarchive import TitanArchive

with TitanArchive('test.zip') as ta:
    properties = ta.GetArchiveItemPropertiesByPath('dir1\\another_file.txt')
    data = ta.ExtractArchiveItemToBufferByIndex(properties.Index)
    print(data.read())
```
```console
b'Test Data 123'
```
