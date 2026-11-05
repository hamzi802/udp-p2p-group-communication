# Initial executable/build checks only; add module and integration tests as features land.
add_test(NAME peer_help COMMAND peer --help)
set_tests_properties(peer_help PROPERTIES PASS_REGULAR_EXPRESSION "project scaffold")
add_test(NAME peer_version COMMAND peer --version)
set_tests_properties(peer_version PROPERTIES PASS_REGULAR_EXPRESSION "0.1.0")
add_test(NAME peer_rejects_unsupported_option COMMAND peer --not-a-real-option)
set_tests_properties(peer_rejects_unsupported_option PROPERTIES WILL_FAIL TRUE)

