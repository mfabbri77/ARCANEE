#!/bin/sh
# Generic patch to fix cmake_minimum_required warnings
echo "Patching cmake_minimum_required in $(pwd)..."
sed -i 's/cmake_minimum_required(VERSION .*)/cmake_minimum_required(VERSION 3.10)/' CMakeLists.txt
