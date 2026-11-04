add_executable(peer main.cpp)
target_link_libraries(peer PRIVATE cn::common cn::rudp cn::membership cn::chat cn::filestore cn::filetransfer)
