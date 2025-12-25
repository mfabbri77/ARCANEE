#!/bin/sh
# Generic patch to fix cmake_minimum_required warnings
echo "Patching cmake_minimum_required in $(pwd)..."
find . -name "CMakeLists.txt" -exec sed -i -E 's/cmake_minimum_required\s*\(VERSION.*\)/cmake_minimum_required(VERSION 3.10)/g' {} +

