#!/bin/sh

# Fix cmake_minimum_required warnings
find . -name "CMakeLists.txt" -exec sed -i 's/cmake_minimum_required(VERSION .*)/cmake_minimum_required(VERSION 3.10)/g' {} +
