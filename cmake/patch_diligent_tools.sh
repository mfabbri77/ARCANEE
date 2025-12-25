#!/bin/sh

# Original PATCH_COMMAND logic from CMakeLists.txt
sed -i 's/add_subdirectory(RenderStateNotation)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(NativeApp)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(HLSL2GLSLConverter)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(RenderStatePackager)/#&/' CMakeLists.txt
sed -i 's/target_link_libraries(Diligent-Imgui PRIVATE XCBKeySyms)/#&/' Imgui/CMakeLists.txt

# Fix cmake_minimum_required warnings
find . -name "CMakeLists.txt" -exec sed -i 's/cmake_minimum_required(VERSION .*)/cmake_minimum_required(VERSION 3.10)/g' {} +


# Silence PNG_BUILD_ZLIB deprecation in libpng
# It says "use ZLIB_ROOT instead". We can try to set it or just accept it.
# Or we can patch the message() call to not be a warning?
# Let's just sed comment out the warning if possible, or update the logic.
# The warning is: 'The option PNG_BUILD_ZLIB has been deprecated; please use ZLIB_ROOT instead'
sed -i 's/message(WARNING "The option PNG_BUILD_ZLIB has been deprecated/message(STATUS "The option PNG_BUILD_ZLIB has been deprecated/' ThirdParty/libpng/CMakeLists.txt
