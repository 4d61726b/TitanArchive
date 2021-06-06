import setuptools
import os
import subprocess
import tempfile
import shutil
import platform
from wheel.bdist_wheel import bdist_wheel

VERSION = '1.0'

if os.name == 'nt':
    import vswhere

#####################
BUILD_DEBUG = False
src_files = ['P7Zip.cpp', 'TitanArchive.cpp', 'Compat.cpp']
os_libs = []
if os.name == 'nt':
    os_libs += ['OleAut32.lib']
elif os.name == 'posix':
    os_libs += ['-ldl']
SETUP_DIR = os.path.dirname(os.path.realpath(__file__))
PACKAGE_DIR = os.path.join(SETUP_DIR, 'titanarchive')
BIN_DIR = os.path.join(PACKAGE_DIR, 'bin')
NATIVE_DIR = os.path.realpath(os.path.join(SETUP_DIR, '..', 'Native'))
#####################

if 'microsoft' in platform.uname()[3].lower() and SETUP_DIR.startswith('/mnt/c'):
    raise Exception('setup.py should not be run within a windows mount because of permission issues')

src_files = [os.path.join(NATIVE_DIR, path) for path in src_files]
bin_files = []

if BUILD_DEBUG:
    print('DEBUG ' * 500)

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

def windows_build(arch):
    vs_path = vswhere.find(latest=True, requires='Microsoft.VisualStudio.Component.VC.Tools.x86.x64', prop='installationPath')
    if len(vs_path) == 0:
        raise Exception('Unable to discover installation path of Visual Studio C/C++ build tools')

    vs_path = vs_path[0]
    msvc_path = os.path.join(vs_path, 'VC\\Tools\\MSVC\\', open(os.path.join(vs_path, 'VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt'),'r').read().rstrip())

    host_arch = 'x64' if 'PROGRAMFILES(X86)' in os.environ else 'x86'
    cl_path32 = os.path.join(msvc_path, 'bin\\Host{}\\x86\\cl.exe'.format(host_arch))
    cl_path64 = os.path.join(msvc_path, 'bin\\Host{}\\x64\\cl.exe'.format(host_arch))
    cl_args = ['/LD', '/EHsc', '/MT', '/MP', '/W4', '/WX']

    if BUILD_DEBUG:
        cl_args += ['/Od', '/DEBUG', '/Z7']
    else:
        cl_args.append('/O2')
    
    with tempfile.TemporaryDirectory() as tmpdirname:
        cl_args.append('/Fo{}\\'.format(tmpdirname))
        
        if arch == 32:
            windows_set_env(vs_path, 32)
            bin_file = os.path.join(BIN_DIR, 'TitanArchive32.dll')
            subprocess.run([cl_path32, *cl_args, '/Fe:{}'.format(bin_file), *src_files, *os_libs], check=True)
        elif arch == 64:
            windows_set_env(vs_path, 64)
            bin_file = os.path.join(BIN_DIR, 'TitanArchive64.dll')
            subprocess.run([cl_path64, *cl_args, '/Fe:{}'.format(bin_file), *src_files, *os_libs], check=True)
        else:
            raise Exception('Unknown architecture specified')
        
        bin_files.append(os.path.relpath(bin_file))

def linux_build(arch):
    gpp_path = shutil.which('g++')
    gpp_args = ['-shared', '-fPIC', '-Wall', '-std=c++14']
    
    if BUILD_DEBUG:
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
    bin_files.append(bin_file)
    
class TitanArchiveBDistWheel(bdist_wheel):
    def run (self):
        if self.plat_name == 'win-amd64':
            windows_build(64)
        elif self.plat_name == 'win32':
            windows_build(32)
        elif self.plat_name == 'linux-x86_64':
            linux_build(64)
        elif self.plat_name == 'linux-x86':
            linux_build(32)
        else:
            raise Exception('Unknown build platform specified')
        bdist_wheel.run(self)

setuptools.setup(
    name = 'titanarchive',
    version = VERSION,
    description = 'An easy to use multiplatform archive extraction wrapper library for 7-Zip',
    license = 'LGPL-2.1',
    packages = setuptools.find_packages(where=SETUP_DIR),
    package_dir = {'': SETUP_DIR},
    python_requires = '>=3',
    cmdclass = {'bdist_wheel': TitanArchiveBDistWheel},
    data_files = [('DLLs' if os.name == 'nt' else 'lib', bin_files)]
)
