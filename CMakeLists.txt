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

# Avvisi del compilatore, solo sul codice di K-Zero (non sulle librerie esterne).
# KZERO_WERROR=ON trasforma ogni avviso in errore: lo usa il controllo su GitHub.
option(KZERO_WERROR "Tratta gli avvisi come errori" OFF)
if(MSVC)
  set(KZERO_WARNINGS /W4)
  if(KZERO_WERROR)
    list(APPEND KZERO_WARNINGS /WX)
  endif()
else()
  set(KZERO_WARNINGS -Wall -Wextra -Wpedantic)
  if(KZERO_WERROR)
    list(APPEND KZERO_WARNINGS -Werror)
  endif()
endif()
target_compile_options(kzero PRIVATE ${KZERO_WARNINGS})

# Prove automatiche delle regole del mestiere: ctest --test-dir build --output-on-failure
# GoogleTest a versione fissata, verificata con l'impronta del file: la compilazione resta uguale nel tempo.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # Windows: stessa libreria di sistema del programma
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(test_regole tests/test_regole.cpp)
target_include_directories(test_regole PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_regole PRIVATE ${KZERO_WARNINGS})
target_link_libraries(test_regole PRIVATE GTest::gtest_main)

# Ogni TEST di tests/ viene registrato da solo: nessun elenco da tenere aggiornato qui.
include(GoogleTest)
gtest_discover_tests(test_regole)

# OR-Tools verra' collegato nel passo successivo:
# find_package(ortools CONFIG REQUIRED)
# target_link_libraries(kzero PRIVATE ortools::ortools)
