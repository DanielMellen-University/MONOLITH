cmake_minimum_required(VERSION 3.16)
project(Monolith VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2
find_package(SDL2 REQUIRED)

add_executable(monolith
    src/main.cpp
)

target_link_libraries(monolith
    PRIVATE
        SDL2::SDL2
)

# Helpful warnings
if(MSVC)
    target_compile_options(monolith PRIVATE /W4)
else()
    target_compile_options(monolith PRIVATE -Wall -Wextra -Wpedantic)
endif()
