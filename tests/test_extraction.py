import unittest
import titanarchive
import os
import tempfile
import zipfile
import time
import datetime
import math
from enum import Enum, auto

ARCHIVE_PATH = os.path.dirname(os.path.realpath(__file__))
TEST_ZIP = os.path.join(ARCHIVE_PATH, 'archives', 'Test.zip')
PW_TEST_ZIP = os.path.join(ARCHIVE_PATH, 'archives', 'Pw.zip')

if os.name == 'nt':
    SLASH_CHAR = '\\'
else:
    SLASH_CHAR = '/'

class ArchiveInputType(Enum):
    MEMORY = auto()
    DISK = auto()
    FD = auto()

def filetime_to_unix_epoch(ft):
    EPOCH_AS_FILETIME = 116444736000000000
    us = (ft - EPOCH_AS_FILETIME) // 10
    return math.ceil(datetime.timedelta(microseconds = us).total_seconds())

def open_with_invalid_format(cls, input_type):
    try:
        extract_and_verify(cls, input_type, TEST_ZIP, 'invalidformat')
        raise Exception('Unreachable')
    except titanarchive.TitanArchiveException:
        pass

def open_with_wrong_format(cls, input_type):
    try:
        extract_and_verify(cls, input_type, TEST_ZIP, 'rar')
        raise Exception('Unreachable')
    except titanarchive.TitanArchiveException:
        pass

def extract_and_verify(cls, input_type, zip_on_disk, archive_format = None, password = None):
    if input_type == ArchiveInputType.MEMORY:
        with open(zip_on_disk, 'rb') as f:
            data = f.read()
            ta = titanarchive.TitanArchive(data, password, archive_format)
            _extract_and_verify(cls, zip_on_disk, ta, password)
    elif input_type == ArchiveInputType.DISK:
        ta = titanarchive.TitanArchive(zip_on_disk, password, archive_format)
        _extract_and_verify(cls, zip_on_disk, ta, password)
    elif input_type == ArchiveInputType.FD:
        fd = os.open(zip_on_disk, os.O_RDONLY | (os.O_BINARY if os.name == 'nt' else 0))
        try:
            ta = titanarchive.TitanArchive(fd, password, archive_format)
        finally:
            os.close(fd)
        _extract_and_verify(cls, zip_on_disk, ta, password)
    else:
        raise Exception('Unknown type specified')

def _extract_and_verify(cls, zip_file, ta, password):
    with tempfile.TemporaryDirectory() as tmp_dir:
        with zipfile.ZipFile(zip_file, 'r') as z:
            if password is not None:
                z.setpassword(password.encode())
            z.extractall(tmp_dir)
            for f in z.infolist():
                name, date_time, = f.filename, f.date_time
                name = os.path.join(tmp_dir, os.path.normpath(name))
                date_time = time.mktime(date_time + (0, 0, -1))
                os.utime(name, (date_time, date_time))
            cls.assertEqual(compare(cls, tmp_dir, ta, ''), ta.GetArchiveItemCount())

def _compare_archive_items(cls, ai1, ai2):
    cls.assertEqual(ai1.Index, ai2.Index)
    if SLASH_CHAR in ai1.Path and SLASH_CHAR not in ai2.Path:
        cls.assertEqual(os.path.basename(ai1.Path), ai2.Path)
    elif SLASH_CHAR in ai2.Path and SLASH_CHAR not in ai1.Path:
        cls.assertEqual(ai1.Path, os.path.basename(ai2.Path))
    else:
        cls.assertEqual(ai1.Path, ai2.Path)
    cls.assertEqual(ai1.IsDir, ai2.IsDir)
    cls.assertEqual(ai1.Size, ai2.Size)
    cls.assertEqual(ai1.MTime, ai2.MTime)

def compare(cls, disk_dir, ta, curr_dir):
    cls.assertEqual(len(os.listdir(disk_dir)), len(ta.ListDirectory(curr_dir)))
    total_elements = 0
    for entry in os.listdir(disk_dir):
        full_entry = os.path.join(disk_dir, entry)
        found = False
        for entry2 in ta.ListDirectory(curr_dir):
            if entry == entry2.Path:
                cls.assertEqual(os.path.isdir(full_entry), entry2.IsDir)
                if not os.path.isdir(full_entry):
                    cls.assertEqual(os.path.getsize(full_entry), entry2.Size)
                cls.assertAlmostEqual(os.path.getmtime(full_entry), filetime_to_unix_epoch(entry2.MTime), delta=1)
                if os.path.isfile(full_entry):
                    with open(full_entry, 'rb') as f:
                        data1 = f.read()
                        data2 = ta.ExtractArchiveItemToBufferByPath(os.path.join(curr_dir, entry2.Path))
                        cls.assertEqual(data1, data2.getvalue())
                        data2 = ta.ExtractArchiveItemToBufferByIndex(entry2.Index)
                        cls.assertEqual(data1, data2.getvalue())
                        _compare_archive_items(cls, ta.GetArchiveItemPropertiesByIndex(entry2.Index), entry2)
                        _compare_archive_items(cls, ta.GetArchiveItemPropertiesByPath(os.path.join(curr_dir, entry2.Path)), entry2)
                found = True
                break
        cls.assertTrue(found)
        total_elements += 1
        if os.path.isdir(full_entry):
            total_elements += compare(cls, full_entry, ta, os.path.join(curr_dir, entry))
    return total_elements

class ExtractionTests(unittest.TestCase):
    @classmethod
    def setUp(self):
        titanarchive.GlobalInitialize()
    @classmethod
    def tearDown(self):
        titanarchive.GlobalUninitialize()

    def test_GlobalInitMulti(self):
        titanarchive.GlobalUninitialize()
        titanarchive.GlobalInitialize()
        titanarchive.GlobalInitialize()
    def test_GlobalUninitMulti(self):
        titanarchive.GlobalUninitialize()
        titanarchive.GlobalUninitialize()
    def test_GlobalInitPath(self):
        if os.name == 'nt':
            titanarchive.GlobalInitialize('C:\\Program Files\\7-Zip\\7z.dll')
        elif os.name == 'posix':
            titanarchive.GlobalInitialize('/usr/lib/p7zip/7z.so')
        else:
            raise Exception('Unknown Platform')
        extract_and_verify(self, ArchiveInputType.MEMORY, TEST_ZIP)
    def test_OpenFromMemory(self):
        extract_and_verify(self, ArchiveInputType.MEMORY, TEST_ZIP)
        extract_and_verify(self, ArchiveInputType.MEMORY, TEST_ZIP, 'zip')
        extract_and_verify(self, ArchiveInputType.MEMORY, PW_TEST_ZIP, password = 'password')
        extract_and_verify(self, ArchiveInputType.MEMORY, TEST_ZIP, password = 'password')
        open_with_invalid_format(self, ArchiveInputType.MEMORY)
        open_with_wrong_format(self, ArchiveInputType.MEMORY)
    def test_OpenFromDisk(self):
        extract_and_verify(self, ArchiveInputType.DISK, TEST_ZIP)
        extract_and_verify(self, ArchiveInputType.DISK, TEST_ZIP, 'zip')
        extract_and_verify(self, ArchiveInputType.DISK, PW_TEST_ZIP, password = 'password')
        extract_and_verify(self, ArchiveInputType.DISK, TEST_ZIP, password = 'password')
        open_with_invalid_format(self, ArchiveInputType.DISK)
        open_with_wrong_format(self, ArchiveInputType.DISK)
    def test_OpenFromFD(self):
        extract_and_verify(self, ArchiveInputType.FD, TEST_ZIP)
        extract_and_verify(self, ArchiveInputType.FD, TEST_ZIP, 'zip')
        extract_and_verify(self, ArchiveInputType.FD, PW_TEST_ZIP, password = 'password')
        extract_and_verify(self, ArchiveInputType.FD, TEST_ZIP, password = 'password')
        open_with_invalid_format(self, ArchiveInputType.FD)
        open_with_wrong_format(self, ArchiveInputType.FD)
    def test_GetArchiveFormat(self):
        with titanarchive.TitanArchive(TEST_ZIP) as ta:
            self.assertEqual(ta.GetArchiveFormat(), 'zip')
    def test_PasswordIterations(self):
        i = 0
        with titanarchive.TitanArchive(PW_TEST_ZIP) as ta:
            try:
                while True:
                    if not ta.GetArchiveItemPropertiesByIndex(i).IsDir:
                        break
                    i += 1
            except titanarchive.TitanArchiveException:
                raise Exception('No files discovered in archive')
        with titanarchive.TitanArchive(PW_TEST_ZIP) as ta:
            data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'password')
            try:
                data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'wrongpassword')
            except titanarchive.TitanArchiveException:
                pass
            else:
                raise Exception('Unreachable')
            try:
                data = ta.ExtractArchiveItemToBufferByIndex(i)
            except titanarchive.TitanArchiveException:
                pass
            else:
                raise Exception('Unreachable')
        with titanarchive.TitanArchive(PW_TEST_ZIP, password = 'wrongpassword') as ta:
            data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'password')
            try:
                data = ta.ExtractArchiveItemToBufferByIndex(i)
            except titanarchive.TitanArchiveException:
                pass
            else:
                raise Exception('Unreachable')
            try:
                data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'wrongpassword')
            except titanarchive.TitanArchiveException:
                pass
            else:
                raise Exception('Unreachable')
        with titanarchive.TitanArchive(PW_TEST_ZIP, password = 'password') as ta:
            data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'password')
            data = ta.ExtractArchiveItemToBufferByIndex(i)
            try:
                data = ta.ExtractArchiveItemToBufferByIndex(i, password = 'wrongpassword')
            except titanarchive.TitanArchiveException:
                pass
            else:
                raise Exception('Unreachable')
    def test_MiscInvalid(self):
        with titanarchive.TitanArchive(TEST_ZIP) as ta:
            try:
                ta.ListDirectory('Invalid Directory')
                raise Exception('Unreachable')
            except titanarchive.TitanArchiveException:
                pass
            try:
                ta.GetArchiveItemPropertiesByPath('Invalid Path')
                raise Exception('Unreachable')
            except titanarchive.TitanArchiveException:
                pass
            try:
                ta.GetArchiveItemPropertiesByIndex(99999999)
                raise Exception('Unreachable')
            except titanarchive.TitanArchiveException:
                pass
            try:
                ta.ExtractArchiveItemToBufferByPath('Invalid Path')
                raise Exception('Unreachable')
            except titanarchive.TitanArchiveException:
                pass
            try:
                ta.ExtractArchiveItemToBufferByIndex(99999999)
                raise Exception('Unreachable')
            except titanarchive.TitanArchiveException:
                pass
    
    def test_CloseArchiveAndException(self):
        ta = titanarchive.TitanArchive(TEST_ZIP)    
        ta.ListDirectory('')
        ta.CloseArchive()
        try:
            ta.ListDirectory('')
            raise Exception('Unreachable')
        except titanarchive.TitanArchiveException:
            pass
                    
if __name__ == '__main__':
    unittest.main(verbosity=2)
    