import os
import tempfile
import timeit
import zipfile
import titanarchive

zip_file_path = None

EXTRACTION_ITERATIONS = 10

def titanarchive_extract():
    with titanarchive.TitanArchive(zip_file_path) as ta:
        for i in range(0, EXTRACTION_ITERATIONS):
            for i in range(0, ta.GetArchiveItemCount()):
                ta.ExtractArchiveItemToBufferByIndex(i)

def zipfile_extract():
    with zipfile.ZipFile(zip_file_path, 'r') as z:
        for i in range(0, EXTRACTION_ITERATIONS):
            for name in z.namelist():
                z.read(name)

def create_file(path, size):
    CHUNK_SIZE = 1024 * 1024
    with open(path, 'wb') as f:
        while size > 0:
            if CHUNK_SIZE > size:
                bytes_to_write = size
            else:
                bytes_to_write = CHUNK_SIZE
            f.write(os.urandom(bytes_to_write))
            size -= bytes_to_write
    
def benchmark_file(size):
    global zip_file_path
    print('Benchmarking...')
    with tempfile.TemporaryDirectory() as tmp_dir:
        large_file_path = os.path.join(tmp_dir, 'file')
        create_file(large_file_path, size)
        with tempfile.TemporaryDirectory() as tmp_dir2:
            zip_file_path = os.path.join(tmp_dir2, 'file.zip')
            with zipfile.ZipFile(zip_file_path, 'w') as z:
                z.write(large_file_path)
            print('TitanArchive: ' + str(timeit.timeit('titanarchive_extract()', globals=globals(), number=1)) + 'sec')
            print('ZipFile: ' + str(timeit.timeit('zipfile_extract()', globals=globals(), number=1)) + 'sec')
            zip_file_path = None

def benchmark_many_files():
    pass

if __name__ == '__main__':
    benchmark_file(1024*1024*1024)