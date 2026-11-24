# Owner: M4. Do not replace real feature checks with placeholder passing tests.
add_executable(filetransfer_block_assembler_test test_block_assembler.cpp)
target_link_libraries(filetransfer_block_assembler_test PRIVATE cn::filetransfer)
add_test(NAME filetransfer_block_assembler COMMAND filetransfer_block_assembler_test)
