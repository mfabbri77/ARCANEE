#!/bin/sh
# Fix alias bug in sq/CMakeLists.txt
sed -i "s/ALIAS sq)/ALIAS sq_static)/g" sq/CMakeLists.txt

# Disable export(EXPORT ...) command block
sed -i '/^export(EXPORT/,/  )/s/^/#/' CMakeLists.txt

# Disable install(EXPORT ...) command block
sed -i '/^  install(EXPORT/,/    )/s/^/#/' CMakeLists.txt

# Fix CMake deprecation warning
# Remove deprecated -gstabs flag
find . -name "CMakeLists.txt" -exec sed -i 's/-gstabs//g' {} +


