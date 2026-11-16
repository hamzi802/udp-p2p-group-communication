# Owner: M4. Do not replace real feature checks with placeholder passing tests.
add_executable(filetransfer_scheduler_test test_scheduler.cpp)
target_link_libraries(filetransfer_scheduler_test PRIVATE cn::filetransfer)
add_test(NAME filetransfer_scheduler COMMAND filetransfer_scheduler_test)
