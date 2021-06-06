#!/bin/bash

set -e

cd $(dirname "$0")
./buildall.sh unix_test_env

cd unix_test_env

bin/python3 -m pip install --no-index --find-links=../../Binaries titanarchive
bin/python3 tasrc/Python/tests/benchmark.py
