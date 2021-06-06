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
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
    # Do actions
```

#### Open archive from memory buffer:
```python
import titanarchive

data = open('test.zip', 'rb').read()

with titanarchive.TitanArchive(data) as ta:
    # Do actions
```

#### Open archive from file descriptor:
```python
import os
import titanarchive

fd = os.open('test.zip', os.O_RDONLY | (os.O_BINARY if os.name == 'nt' else 0))

with titanarchive.TitanArchive(fd) as ta:
    # Do actions
```

#### Discover archive format:
```python
import titanarchive

with titanarchive.TitanArchive('unknown.dat') as ta:
    print('Format: {}'.format(ta.GetArchiveFormat()))
```
```console
Format: zip
```

#### Print all files and directories in archive (Method 1, by path):
```python
import titanarchive
import os

def print_files(ta, path):
    for item in ta.ListDirectory(path):
        full_path = os.path.join(path, item.Path)
        if item.IsDir:
            print_files(ta, full_path)
        print('Item: {}'.format(full_path))

with titanarchive.TitanArchive('test.zip') as ta:
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
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
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
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
    data = ta.ExtractArchiveItemToBufferByPath('dir1\\another_file.txt')
    print(data.read())
```
```console
b'Test Data 123'
```

#### Extract file to memory (Method 2, by index):
```python
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
    properties = ta.GetArchiveItemPropertiesByPath('dir1\\another_file.txt')
    data = ta.ExtractArchiveItemToBufferByIndex(properties.Index)
    print(data.read())
```
```console
b'Test Data 123'
```
