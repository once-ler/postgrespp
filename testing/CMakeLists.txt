cmake_minimum_required(VERSION 3.2)

project(postgrespp_test VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.62.0 COMPONENTS system log log_setup)

include_directories("${PROJECT_BINARY_DIR}/../postgrespp" "/usr/include/postgresql")
link_directories("${PROJECT_BINARY_DIR}/../lib")

add_executable(postgrespp_test main.cpp)

SET_TARGET_PROPERTIES(postgrespp_test PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(postgrespp_test postgrespp pq ${Boost_LIBRARIES})
