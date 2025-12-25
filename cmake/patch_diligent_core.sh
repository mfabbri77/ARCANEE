#!/bin/sh

# 1. Update cmake_minimum_required to 3.10 (handle optional spaces)
find . -name "CMakeLists.txt" -exec sed -i -E 's/cmake_minimum_required\s*\(VERSION.*\)/cmake_minimum_required(VERSION 3.10)/g' {} +

# 2. Inject Policy suppressions
# We match the new version line we just set.
# CMP0072 NEW: Prefer GLVND (Fixes deprecation of OLD policy)
# CMP0175 OLD: Allow custom command (Fixes dev warning)
# CMP0177 OLD: Relax install paths (Fixes dev warning)
find . -name "CMakeLists.txt" -exec sed -i '/cmake_minimum_required(VERSION 3.10)/a if(POLICY CMP0175) \n  cmake_policy(SET CMP0175 OLD) \nendif() \nif(POLICY CMP0177) \n  cmake_policy(SET CMP0177 OLD) \nendif() \nif(POLICY CMP0072) \n  cmake_policy(SET CMP0072 NEW) \nendif()' {} +

# 3. Silence DiligentCore GCC optimization warning
# The warning is manually emitted by message(WARNING ...). We turn it into STATUS or comment it out.
find . -name "CMakeLists.txt" -exec sed -i 's/message(WARNING "gcc 9.0 and above/message(STATUS "gcc 9.0 and above/g' {} +

# 4. Disable warnings (and thus warnings-as-errors) globally for DiligentCore
# We inject add_compile_options(-w) right after cmake_minimum_required
find . -name "CMakeLists.txt" -exec sed -i '/cmake_minimum_required(VERSION 3.10)/a add_compile_options(-w)' {} +
