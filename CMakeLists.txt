cmake_minimum_required(VERSION 3.17)
project(SearchServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(SearchServer main.cpp document.h document.cpp log_duration.h paginator.h process_queries.h process_queries.cpp read_input_functions.cpp read_input_functions.h request_queue.cpp request_queue.h search_server.cpp search_server.h string_processing.cpp string_processing.h test_example_functions.cpp request_queue.h concurrent_map.h)
target_link_libraries(SearchServer PRIVATE -ltbb -lpthread)