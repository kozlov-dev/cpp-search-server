cmake_minimum_required(VERSION 3.16)
project(final_FindCORE_v2)

set(CMAKE_CXX_STANDARD 17)

add_executable(final_FindCORE_v2 main.cpp paginator.h document.cpp document.h read_input_functions.cpp read_input_functions.h request_queue.cpp request_queue.h string_processing.cpp string_processing.h search_server.cpp search_server.h)