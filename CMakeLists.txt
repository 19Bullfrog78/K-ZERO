cmake_minimum_required(VERSION 3.25)
project(KZero VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

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

# --- Librerie esterne ------------------------------------------------------------
# Versione fissata e impronta (SHA256) del file scaricato: la compilazione resta uguale nel tempo.
# SYSTEM: gli avvisi delle librerie non contano come avvisi di K-Zero.
include(FetchContent)
# CLI11 2.7.2: lettura dei parametri della riga di comando (un solo file).
FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/releases/download/v2.7.2/CLI11.hpp
  URL_HASH SHA256=ffa9a30da295c5858fb5f91f9f45771bab09471d7010a34c7c68c857a330dd76
  DOWNLOAD_NO_EXTRACT TRUE
)
# nlohmann/json 3.12.0: lettura e scrittura del JSON.
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM
)
# GoogleTest 1.15.2: prove automatiche.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM
)
set(JSON_ImplicitConversions OFF CACHE BOOL "" FORCE)   # conversioni dal JSON solo esplicite
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)      # Windows: stessa libreria di sistema del programma
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cli11 nlohmann_json googletest)

add_library(kzero_cli11 INTERFACE)
target_include_directories(kzero_cli11 SYSTEM INTERFACE ${cli11_SOURCE_DIR})

# --- Piano 1: nucleo (modello dati, regole, calcolo) -------------------------------
# Nessuna stampa, nessuna riga di comando, nessuna libreria esterna (lo controlla una prova).
add_library(kzero_nucleo STATIC src/nucleo/dimostrazione.cpp)
target_include_directories(kzero_nucleo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/nucleo)
target_compile_options(kzero_nucleo PRIVATE ${KZERO_WARNINGS})

# --- Piano 2: API = riga di comando con uscita testo/JSON, schemi ed errori K-ERR ----
configure_file(src/api/versione.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generato/versione.hpp @ONLY)
add_library(kzero_api STATIC
  src/api/comandi.cpp
  src/api/comando_descrivi.cpp
  src/api/comando_dimostrazione.cpp
  src/api/comando_versione.cpp
  src/api/errori.cpp
  src/api/esegui.cpp
  src/api/parametri.cpp
  src/api/uscita.cpp
)
target_include_directories(kzero_api
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/api
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generato
)
target_link_libraries(kzero_api PUBLIC kzero_nucleo nlohmann_json::nlohmann_json PRIVATE kzero_cli11)
target_compile_options(kzero_api PRIVATE ${KZERO_WARNINGS})

# --- Il programma: un solo eseguibile, kzero <comando> [parametri] -----------------
add_executable(kzero src/main.cpp)
target_link_libraries(kzero PRIVATE kzero_api)
target_compile_options(kzero PRIVATE ${KZERO_WARNINGS})

# --- Prove: ctest --test-dir build --output-on-failure -----------------------------
enable_testing()
add_subdirectory(tests)

# OR-Tools verra' collegato al nucleo nel passo del calcolo:
# find_package(ortools CONFIG REQUIRED)
# target_link_libraries(kzero_nucleo PRIVATE ortools::ortools)
