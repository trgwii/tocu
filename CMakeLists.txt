cmake_minimum_required(FATAL_ERROR VERSION 3.10)

project(toc_updater C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(tocu main.c)

if(APPLE OR LINUX)
    set(CMAKE_C_FLAGS_DEBUG -fsanitize=address)
    target_compile_options(tocu PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wformat=2
        -Wuninitialized
        -Wstrict-overflow=5
        -Wshadow
        -Wwrite-strings
        -Wcast-qual
        -Wconversion
    )
elseif(WIN32)
    target_compile_options(tocu PRIVATE
        /Wall
        /WX
        /wd4710 # "function not inlined"
        /wd4711 # "function selected for automatic inline expansion"
        /wd4820 # -Wno-padded
        /wd5045 # "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"
    )
endif()

set_target_properties(tocu PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    LINK_FLAGS_RELEASE -s
)
