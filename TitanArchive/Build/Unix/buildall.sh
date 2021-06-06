#!/bin/bash

set -e

cd $(dirname "$0")

if [ -z "$1" ]
then
    venv="unix_build_env"        
else
    venv=$1
fi

rm -rf $venv

python3 -m venv --system-site-packages $venv

cd $venv

mkdir tasrc

cp -r ../../../Python tasrc/
cp -r ../../../Native tasrc/

bin/python3 tasrc/Python/setup.py bdist_wheel --plat-name=linux-x86_64
bin/python3 tasrc/Python/setup.py bdist_wheel --plat-name=linux-x86

cp tasrc/Python/titanarchive/bin/* ../../Binaries/
cp dist/* ../../Binaries/
