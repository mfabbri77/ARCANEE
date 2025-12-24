
# Enable CTest
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Test Executable
add_executable(arcanee_tests
    test_vfs_sandbox.cpp
    test_determinism_replay.cpp
    test_script_safety.cpp
    test_script_safety.cpp
    test_render_smoke.cpp
    test_audio_queue.cpp
)

# Link against engine components
target_link_libraries(arcanee_tests
    PRIVATE
    arcanee_core
    gtest_main
)

# Include directories
target_include_directories(arcanee_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

# Register tests
include(GoogleTest)
gtest_discover_tests(arcanee_tests)
