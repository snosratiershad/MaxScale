# On too many platforms curling to a non-existent site
# does not result in a proper error. Disabled for the
# time being.
#
#add_executable(test_mxb_http test_http.cc)
#target_link_libraries(test_mxb_http
#  maxbase
#  ${JANSSON_LIBRARIES}
#  rt
#)
#add_test(test_mxb_http test_mxb_http)

add_executable(test_average test_average.cc)
target_link_libraries(test_average maxbase)
add_test(test_average test_average)

add_executable(test_host_class test_host_class.cc)
target_link_libraries(test_host_class maxbase)
add_test(test_host_class test_host_class)

add_executable(test_mxb_log test_log.cc)
target_link_libraries(test_mxb_log maxbase)
add_test(test_mxb_log test_mxb_log)

add_executable(test_mxb_string test_string.cc)
target_link_libraries(test_mxb_string maxbase pthread)
add_test(test_mxb_string test_mxb_string)

add_executable(test_name_lookup test_name_lookup.cc)
target_link_libraries(test_name_lookup maxbase)
add_test(test_name_lookup test_name_lookup)

add_executable(test_semaphore test_semaphore.cc)
target_link_libraries(test_semaphore maxbase)
add_test(test_semaphore test_semaphore)

add_executable(test_system test_system.cc)
target_link_libraries(test_system maxbase)
add_test(test_system test_system)

add_executable(test_worker test_worker.cc)
target_link_libraries(test_worker maxbase)
add_test(test_worker test_worker)

add_executable(test_threadpool test_threadpool.cc)
target_link_libraries(test_threadpool maxbase)
add_test(test_threadpool test_threadpool)

add_executable(test_ini_parser test_ini_parser.cc)
target_link_libraries(test_ini_parser maxbase)
add_test(test_ini_parser test_ini_parser)

add_executable(test_encryption test_encryption.cc)
target_link_libraries(test_encryption maxbase crypto)
add_test(test_encryption test_encryption)

add_executable(test_latch test_latch.cc)
target_link_libraries(test_latch maxbase)
add_test(test_latch test_latch)

add_executable(test_proxy_protocol test_proxy_protocol.cc)
target_link_libraries(test_proxy_protocol maxbase)
add_test(test_proxy_protocol test_proxy_protocol)

add_executable(test_small_vector test_small_vector.cc)
target_link_libraries(test_small_vector maxbase)
add_test(test_small_vector test_small_vector)
# For some reason, with GCC 11 the test code causes a -Wstringop-overflow
# warning to be generated in release mode builds if the type of T is
# std::string. Using std::get_if() or turning on either ASAN or UBSAN appears to
# make disappear. Disable the warning for this test as it appears to only repeat
# outside of the build environment and in docker containers.
target_compile_options(test_small_vector PUBLIC -Wno-stringop-overflow)

add_executable(test_collector test_collector.cc)
target_link_libraries(test_collector maxbase)
add_test(test_collector test_collector)
