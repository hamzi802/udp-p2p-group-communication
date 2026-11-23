add_library(cn_filestore STATIC
    src/module.cpp
    src/manifest.cpp
    src/chunk_store.cpp
    src/placement.cpp
    src/wire.cpp
    src/file_store.cpp
)
add_library(cn::filestore ALIAS cn_filestore)
target_include_directories(cn_filestore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cn_filestore PUBLIC cn::common cn::hashing)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
