find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)

add_library(cn_hashing STATIC src/module.cpp)
add_library(cn::hashing ALIAS cn_hashing)
target_include_directories(cn_hashing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cn_hashing PUBLIC cn::common OpenSSL::Crypto)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
