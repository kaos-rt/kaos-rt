cmake_minimum_required(VERSION 3.20)

project(kaos_posix_example C)

find_package(Threads REQUIRED)

add_executable(kaos_posix_example
    main.c
    ../../fsm.c
    ../../port/posix/fsm_port.c
)

target_compile_features(kaos_posix_example PRIVATE c_std_17)

target_include_directories(kaos_posix_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../port/posix
)

target_link_libraries(kaos_posix_example PRIVATE Threads::Threads)
