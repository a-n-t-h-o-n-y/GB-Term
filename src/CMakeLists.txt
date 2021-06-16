cmake_minimum_required(VERSION 3.9)

add_executable(gbemu-term main.cpp)
target_link_libraries(gbemu-term
    PRIVATE
        TermOx
        gbemu-core
)

target_compile_features(gbemu-term
    PUBLIC
        cxx_std_17
)

target_compile_options(gbemu-term
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
