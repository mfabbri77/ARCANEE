#!/bin/sh

# 1. Original fixes for headers
sed -i 's/add_subdirectory(RenderStateNotation)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(NativeApp)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(HLSL2GLSLConverter)/#&/' CMakeLists.txt
sed -i 's/add_subdirectory(RenderStatePackager)/#&/' CMakeLists.txt

# Replace XCBKeySyms target with actual system library (since NativeApp is disabled)
sed -i 's/target_link_libraries(Diligent-Imgui PRIVATE XCBKeySyms)/target_link_libraries(Diligent-Imgui PRIVATE xcb-keysyms xcb)/' Imgui/CMakeLists.txt



# 2. Update cmake_minimum_required to 3.10 (handle optional spaces)
find . \( -name "CMakeLists.txt" -o -name "*.cmake" \) -exec sed -i -E 's/cmake_minimum_required\s*\(VERSION.*\)/cmake_minimum_required(VERSION 3.10)/g' {} +

# 3. Inject Policy suppressions
find . -name "CMakeLists.txt" -exec sed -i '/cmake_minimum_required(VERSION 3.10)/a if(POLICY CMP0175) \n  cmake_policy(SET CMP0175 OLD) \nendif() \nif(POLICY CMP0177) \n  cmake_policy(SET CMP0177 OLD) \nendif() \nif(POLICY CMP0072) \n  cmake_policy(SET CMP0072 NEW) \nendif()' {} +

# Silence libpng deprecation warning (handles multi-line messages by just changing the command)
find . -name "CMakeLists.txt" -exec sed -i 's/message(DEPRECATION/message(STATUS/g' {} +

# 4. Disable warnings (and thus warnings-as-errors) globally for DiligentTools
find . -name "CMakeLists.txt" -exec sed -i '/cmake_minimum_required(VERSION 3.10)/a add_compile_options(-w)' {} +

# 5. Fix hardcoded relative include paths
find . -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) -exec sed -i 's|\.\./\.\./\.\./DiligentCore/|\.\./\.\./\.\./diligentcore-src/|g' {} +

# 6. Fix xcb_keysyms include path (system header is at xcb/xcb_keysyms.h not xcb_keysyms/xcb_keysyms.h)
find . -type f \( -name "*.hpp" -o -name "*.cpp" -o -name "*.h" \) -exec sed -i 's|"xcb_keysyms/xcb_keysyms.h"|<xcb/xcb_keysyms.h>|g' {} +

