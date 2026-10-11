cmake_minimum_required(VERSION 3.16)
project(KZero VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(kzero src/main.cpp)
target_include_directories(kzero PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(kzero PRIVATE /W4)
else()
  target_compile_options(kzero PRIVATE -Wall -Wextra -Wpedantic)
endif()

# OR-Tools verra' collegato nel passo successivo:
# find_package(ortools CONFIG REQUIRED)
# target_link_libraries(kzero PRIVATE ortools::ortools)
