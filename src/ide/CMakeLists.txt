
add_library(arcanee_ide STATIC
    UIShell.cpp
    ProjectSystem.cpp
    DocumentSystem.cpp
    TextBuffer.cpp
    ParseService.cpp
    SearchService.cpp
    TaskRunner.cpp
    TimelineStore.cpp
    LspClient.cpp
    DapClient.cpp
)

# Allow including "ide/..."
target_include_directories(arcanee_ide PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Manually define tree-sitter-squirrel target since it lacks CMake
# if(NOT TARGET tree-sitter-squirrel)
#     set(TS_SRC "${tree-sitter-squirrel_SOURCE_DIR}/src")
#     # Some grammars have scanner.c, some don't.
#     file(GLOB TS_SQUIRREL_SOURCES "${TS_SRC}/parser.c" "${TS_SRC}/scanner.c" "${TS_SRC}/scanner.cc")
    
#     add_library(tree-sitter-squirrel STATIC ${TS_SQUIRREL_SOURCES})
#     target_include_directories(tree-sitter-squirrel PUBLIC "${TS_SRC}")
#     # Fix for missing tree_sitter/parser.h if not in include path
#     # Usually tree-sitter core target provides it.
#     target_link_libraries(tree-sitter-squirrel PRIVATE tree-sitter)
# endif()

# Manually define sqlite3 from amalgamation
# if(NOT TARGET sqlite3)
#     add_library(sqlite3 STATIC "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c")
#     target_include_directories(sqlite3 PUBLIC "${sqlite3_amalgamation_SOURCE_DIR}")
#     target_compile_definitions(sqlite3 PUBLIC SQLITE_OMIT_LOAD_EXTENSION)
#     if(UNIX)
#         target_link_libraries(sqlite3 PRIVATE dl pthread)
#     endif()
# endif()

target_link_libraries(arcanee_ide PUBLIC
    vfs
    common
    # Dependencies
    tree-sitter
    # tree-sitter-squirrel - DEFERRED
    pcre2-8
    # sqlite3 - DEFERRED
    zstd
    xxhash
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_features(arcanee_ide PUBLIC cxx_std_17)
