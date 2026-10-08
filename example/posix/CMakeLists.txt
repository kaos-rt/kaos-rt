cmake_minimum_required(VERSION 3.20)

project(kaos_posix_example C)

find_package(Threads REQUIRED)

option(KAOS_STRICT_WARNINGS "Treat strict compiler diagnostics as errors" ON)

add_executable(kaos_posix_example
    main.c
    ../../fsm.c
    ../../port/posix/fsm_port.c
)

set_target_properties(kaos_posix_example PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

target_include_directories(kaos_posix_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../port/posix
)

target_link_libraries(kaos_posix_example PRIVATE Threads::Threads)

if(KAOS_STRICT_WARNINGS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kaos_posix_example PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wcast-qual
        -Wformat=2
        -Wswitch-enum
    )
endif()
