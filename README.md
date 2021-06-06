# TitanArchive
An easy to use multiplatform archive extractor library

## Features

* Easy API for C and Python. Greatly simplifies the complexity of using [7-Zip](https://www.7-zip.org/)
* Multiplatform. Supports UNIX and Windows
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

#### Open archive from file descriptor
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

#### Print all files and directories in archive (Method 1, by path):
```python
import titanarchive

def print_files(ta, path):
    for item in ta.ListDirectory(path):
        if item.IsDir:
            print_files(ta, item.Path)
        print('Item: {}'.format(item.Path))

with titanarchive.TitanArchive('test.zip') as ta:
    print_files(ta, '')
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

#### Extract file to memory (Method 1, by path):
```python
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
    data = ta.ExtractArchiveItemToBufferByPath('dir1\\another_file.txt')
    print(data)
```

#### Extract file to memory (Method 2, by index):
```python
import titanarchive

with titanarchive.TitanArchive('test.zip') as ta:
    properties = ta.GetArchiveItemPropertiesByPath('dir1\\another_file.txt')
    data = ta.ExtractArchiveItemToBufferByIndex(properties.Index)
    print(data)
```


