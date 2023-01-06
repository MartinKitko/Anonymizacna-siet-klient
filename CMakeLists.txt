cmake_minimum_required(VERSION 3.18)
project(AnonSietKlient C)

set(CMAKE_C_STANDARD 99)

add_executable(AnonSietKlient client_main.c client_definitions.c client_definitions.h)

target_link_libraries(AnonSietKlient pthread)