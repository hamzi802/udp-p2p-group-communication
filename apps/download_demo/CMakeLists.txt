add_executable(download_demo main.cpp)
target_link_libraries(download_demo PRIVATE cn::filetransfer)

if(BUILD_TESTING)
    add_test(NAME download_demo_failover COMMAND download_demo --demo)
    set_tests_properties(download_demo_failover PROPERTIES
        PASS_REGULAR_EXPRESSION "SUCCESS: file rebuilt correctly")
endif()
