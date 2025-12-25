#!/bin/sh

# Fix cmake_minimum_required warnings
sed -i 's/CMAKE_MINIMUM_REQUIRED(VERSION 2.8.5)/CMAKE_MINIMUM_REQUIRED(VERSION 3.10)/' CMakeLists.txt
