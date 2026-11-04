add_library(cn_common INTERFACE)
add_library(cn::common ALIAS cn_common)
target_include_directories(cn_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(cn_common INTERFACE cxx_std_17)
