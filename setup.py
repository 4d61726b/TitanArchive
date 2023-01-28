import setuptools
import os
import subprocess
import tempfile
import shutil
import platform
import json
import hashlib
import tarfile
import sysconfig
from urllib.request import urlretrieve
from wheel.bdist_wheel import bdist_wheel
from setuptools.command.sdist import sdist
from setuptools.command.build_clib import build_clib
from setuptools.command.install import install

VERSION = '2.0'

if os.name == 'nt':
    import vswhere

#####################
src_files = ['P7Zip.cpp', 'TitanArchive.cpp', 'Compat.cpp']
os_libs = []
if os.name == 'nt':
    os_libs += ['OleAut32.lib']
elif os.name == 'posix':
    os_libs += ['-ldl']
SETUP_DIR = os.path.dirname(os.path.relpath(__file__))
DEPS_DIR = os.path.relpath(os.path.join(SETUP_DIR, 'deps'))
SRC_DIR = os.path.relpath(os.path.join(SETUP_DIR, 'src'))
PACKAGE_DIR = os.path.join(SRC_DIR, 'titanarchive')
BIN_DIR = os.path.join(SETUP_DIR, 'build')
#####################

if 'microsoft' in platform.uname()[3].lower() and SETUP_DIR.startswith('/mnt/c'):
    raise Exception('setup.py should not be run within a windows mount because of permission issues')

src_files = [os.path.join(SRC_DIR, path) for path in src_files]
bin_files = []

try:
    os.mkdir(BIN_DIR)
except FileExistsError:
    pass

def windows_set_env(vs_path, arch):
    vs_path = os.path.join(vs_path, 'VC\\Auxiliary\\Build')
    cmd = [os.path.join(vs_path, 'vcvars{}.bat'.format(arch)), '&&', 'set']
    popen = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = popen.communicate()
    if popen.wait() != 0:
        raise Exception(stderr.decode('mbcs'))
    output = stdout.decode('mbcs').split('\r\n')
    os.environ.update(dict((e[0].upper(), e[1]) for e in [p.rstrip().split('=', 1) for p in output] if len(e) == 2))

def windows_build(arch, debug=False):
    vs_path = vswhere.find(latest=True, requires='Microsoft.VisualStudio.Component.VC.Tools.x86.x64', prop='installationPath')
    if len(vs_path) == 0:
        raise Exception('Unable to discover installation path of Visual Studio C/C++ build tools')

    vs_path = vs_path[0]
    msvc_path = os.path.join(vs_path, 'VC\\Tools\\MSVC\\', open(os.path.join(vs_path, 'VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt'),'r').read().rstrip())

    host_arch = 'x64' if 'PROGRAMFILES(X86)' in os.environ else 'x86'
    cl_path32 = os.path.join(msvc_path, 'bin\\Host{}\\x86\\cl.exe'.format(host_arch))
    cl_path64 = os.path.join(msvc_path, 'bin\\Host{}\\x64\\cl.exe'.format(host_arch))
    cl_args = ['/LD', '/EHsc', '/MT', '/MP', '/W4', '/WX']

    if debug:
        cl_args += ['/Od', '/DEBUG', '/Z7']
    else:
        cl_args.append('/O2')

    with tempfile.TemporaryDirectory() as tmpdirname:
        windows_set_env(vs_path, arch)

        extract_deps(tmpdirname)
        p7zip_build_dir = os.path.join(tmpdirname, '7z-src', 'CPP', '7zip', 'Bundles', 'Format7zF')
        subprocess.run(['nmake'], cwd=p7zip_build_dir, check=True)
        bin_file = os.path.join(BIN_DIR, 'TitanArchive{}-7z.dll'.format(arch))
        shutil.copy2(os.path.join(p7zip_build_dir, 'x86' if arch == 32 else 'x64', '7z.dll'), bin_file)
        bin_files.append(os.path.relpath(bin_file))

        cl_args.append('/Fo{}\\'.format(tmpdirname))
        bin_file = os.path.join(BIN_DIR, 'TitanArchive{}.dll'.format(arch))
        subprocess.run([eval('cl_path{}'.format(arch)), *cl_args, '/Fe:{}'.format(bin_file), *src_files, *os_libs], check=True)
        bin_files.append(os.path.relpath(bin_file))


def linux_build(arch, debug=False):
    gpp_path = shutil.which('g++')

    if gpp_path is None or shutil.which('cc') is None:
        raise Exception('Unable to build, missing cc and/or g++')

    with tempfile.TemporaryDirectory() as tmpdirname:
        extract_deps(tmpdirname)
        p7zip_build_dir = os.path.join(tmpdirname, '7z-src', 'CPP', '7zip', 'Bundles', 'Format7zF')
        subprocess.run(['make', '-f', 'makefile.gcc'], cwd=p7zip_build_dir, check=True)
        bin_file = os.path.join(BIN_DIR, 'TitanArchive{}-7z.so'.format(arch))
        shutil.copy2(os.path.join(p7zip_build_dir, '_o', '7z.so'), bin_file)
        bin_files.append(os.path.relpath(bin_file))

    gpp_args = ['-shared', '-fPIC', '-Wall', '-std=c++14']

    if debug:
        gpp_args += ['-O0', '-g3']
    else:
        gpp_args += ['-O3', '-g0']

    if arch == 32:
        gpp_args += ['-m32']
        bin_file = os.path.join(BIN_DIR, 'TitanArchive32.so')
    elif arch == 64:
        bin_file = os.path.join(BIN_DIR, 'TitanArchive64.so')
    else:
        raise Exception('Unknown architecture specified')

    subprocess.run([gpp_path, *gpp_args, *src_files, '-o', bin_file, *os_libs], check=True)
    bin_files.append(os.path.relpath(bin_file))

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            h.update(chunk)
    return h.hexdigest()

def download_deps():
    for dep in json.load(open(os.path.join(DEPS_DIR, 'manifest.json'), 'r')):
        full_path = os.path.join(DEPS_DIR, dep['filename'])
        try:
            if dep['sha256'] == sha256_file(full_path):
                continue
        except:
            pass

        print('Downloading: {}'.format(dep['url']))
        urlretrieve(dep['url'], full_path)

def extract_deps(path):
    for dep in json.load(open(os.path.join(DEPS_DIR, 'manifest.json'), 'r')):
        extract_dir = os.path.join(path, dep['filename'].split('.', 1)[0])
        if dep['filename'].endswith('.tar.xz'):
            os.mkdir(extract_dir)
            with tarfile.open(os.path.join(DEPS_DIR, dep['filename'])) as f:
                f.extractall(extract_dir)
        else:
            raise Exception('Unsupported archive')

def build_code(plat_name, debug):
    if plat_name == 'win-amd64':
        windows_build(64, debug)
    elif plat_name == 'win32':
        windows_build(32, debug)
    elif plat_name == 'linux-x86_64':
        linux_build(64, debug)
    elif plat_name == 'linux-x86':
        linux_build(32, debug)
    else:
        raise Exception('Unknown / Missing build platform specified')

class TitanArchiveInstall(install):
    def run(self):
        build_code(sysconfig.get_platform(), False)
        install.run(self)

class TitanArchiveBuildCLib(build_clib):
    user_options = [('debug', 'g', 'compile with debugging information'), ('platform=', 'p', 'specify the target platform')]

    def run(self):
        build_code(self.platform, False if self.debug is None else True)

    def initialize_options(self):
        build_clib.initialize_options(self)
        self.platform = None

class TitanArchiveBDistWheel(bdist_wheel):
    def run(self):
        build_code(self.plat_name, False)
        bdist_wheel.run(self)

class TitanArchiveSDist(sdist):
    def run(self):
        download_deps()
        sdist.run(self)

setuptools.setup(
    name = 'titanarchive',
    version = VERSION,
    description = 'An easy to use multiplatform archive extraction wrapper library for 7-Zip',
    long_description = open(os.path.join(SETUP_DIR, 'README.md'), 'r').read(),
    long_description_content_type = 'text/markdown',
    license = 'LGPL-2.1',
    url = 'https://github.com/4d61726b/TitanArchive',
    packages = setuptools.find_packages(where=SRC_DIR),
    package_dir = {'': SRC_DIR},
    python_requires = '>=3',
    cmdclass = {'bdist_wheel': TitanArchiveBDistWheel, 'sdist': TitanArchiveSDist, 'build_clib': TitanArchiveBuildCLib, 'install': TitanArchiveInstall},
    data_files = [('DLLs' if os.name == 'nt' else 'lib', bin_files)],
    has_ext_modules = lambda: True
)
