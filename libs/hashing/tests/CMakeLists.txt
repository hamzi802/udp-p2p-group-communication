add_executable(cn_hashing_tests test_sha256.cpp)
target_link_libraries(cn_hashing_tests PRIVATE cn::hashing)
add_test(NAME hashing_behavior COMMAND cn_hashing_tests)
