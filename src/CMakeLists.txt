cmake_minimum_required(VERSION 3.9)

add_executable(gb-term main.cpp)
target_link_libraries(gb-term
    PRIVATE
        TermOx
        gbemu-core
)

target_compile_features(gb-term
    PUBLIC
        cxx_std_17
)

target_compile_options(gb-term
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
