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

# --- Prove di sicurezza (K-009): sanitizer e fuzzing ---------------------------------
# KZERO_SANITIZER=ON: AddressSanitizer (memoria) e UndefinedBehaviorSanitizer (comportamenti non definiti) su tutto,
# librerie comprese; il programma si ferma al primo problema. Lo usa il secondo lavoro del controllo GitHub.
# KZERO_FUZZ=ON (solo clang): in piu' i bersagli del fuzzing con libFuzzer (tests/fuzz, CLAUDE.md).
option(KZERO_SANITIZER "Compila con AddressSanitizer e UndefinedBehaviorSanitizer" OFF)
option(KZERO_FUZZ "Compila anche i bersagli del fuzzing (libFuzzer, solo con clang)" OFF)
if(KZERO_SANITIZER OR KZERO_FUZZ)
  if(MSVC)
    message(FATAL_ERROR "KZERO_SANITIZER e KZERO_FUZZ si usano su Linux con gcc o clang")
  endif()
  set(KZERO_FLAG_SANITIZER -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
  if(KZERO_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "Il fuzzing con libFuzzer richiede clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DKZERO_FUZZ=ON ...")
    endif()
    list(APPEND KZERO_FLAG_SANITIZER -fsanitize=fuzzer-no-link)
  endif()
  add_compile_options(${KZERO_FLAG_SANITIZER})
  add_link_options(-fsanitize=address,undefined)
endif()

# --- Protezioni del compilatore (K-009) per le versioni di rilascio su Linux -----------------
# Rendono piu' difficile sfruttare un errore di memoria (DECISIONI.md): controllo dello stack, controlli delle funzioni
# della libreria C (_FORTIFY_SOURCE, con l'ottimizzazione) e della libreria C++, eseguibile rilocabile (PIE), tabelle
# in sola lettura (RELRO completo), stack non eseguibile. Ogni opzione si usa solo se il compilatore la conosce. Non
# con i sanitizer (che hanno i loro controlli). Le equivalenti per Windows arriveranno con il programma per Windows.
option(KZERO_PROTEZIONI "Protezioni del compilatore nelle versioni di rilascio (Linux)" ON)
set(KZERO_PROTEZIONI_ATTIVE OFF)
string(TOUPPER "${CMAKE_BUILD_TYPE}" tipo_build)
if(KZERO_PROTEZIONI AND NOT KZERO_SANITIZER AND NOT KZERO_FUZZ AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND tipo_build MATCHES "^(RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
  include(CheckCXXCompilerFlag)
  include(CheckLinkerFlag)
  include(CheckPIESupported)
  set(KZERO_PROTEZIONI_ATTIVE ON)
  foreach(opzione -fstack-protector-strong -fstack-clash-protection -fcf-protection)
    string(MAKE_C_IDENTIFIER "KZERO_HA${opzione}" variabile)
    check_cxx_compiler_flag(${opzione} ${variabile})
    if(${variabile})
      add_compile_options(${opzione})
    endif()
  endforeach()
  # Il compilatore puo' gia' definire _FORTIFY_SOURCE (es. Ubuntu): si toglie e si rimette, senza avvisi.
  add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
  add_compile_definitions(_GLIBCXX_ASSERTIONS)
  check_pie_supported()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  check_linker_flag(CXX "LINKER:-z,relro,-z,now" KZERO_HA_RELRO)
  if(KZERO_HA_RELRO)
    add_link_options(LINKER:-z,relro,-z,now)
  endif()
  check_linker_flag(CXX "LINKER:-z,noexecstack" KZERO_HA_NOEXECSTACK)
  if(KZERO_HA_NOEXECSTACK)
    add_link_options(LINKER:-z,noexecstack)
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
# valijson 1.1.3: controllo dei file di dati con gli schemi JSON (draft-07); solo file di intestazione, legge il JSON
# di nlohmann/json con conversioni esplicite. Il progetto non pubblica file di rilascio: la versione e' fissata con
# l'impronta del suo commit (git controlla l'impronta di ogni file scaricato). Il suo CMakeLists non serve:
# SOURCE_SUBDIR punta a una cartella che non esiste, cosi' FetchContent scarica soltanto.
FetchContent_Declare(
  valijson
  GIT_REPOSITORY https://github.com/tristanpenman/valijson.git
  GIT_TAG 5eac4b66f20b2c189ae40281c29e71cf39a958ec   # v1.1.3
  SOURCE_SUBDIR non-usare-il-cmake-di-valijson
)
set(JSON_ImplicitConversions OFF CACHE BOOL "" FORCE)   # conversioni dal JSON solo esplicite
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)      # Windows: stessa libreria di sistema del programma
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cli11 nlohmann_json googletest valijson)

add_library(kzero_cli11 INTERFACE)
target_include_directories(kzero_cli11 SYSTEM INTERFACE ${cli11_SOURCE_DIR})
add_library(kzero_valijson INTERFACE)
target_include_directories(kzero_valijson SYSTEM INTERFACE ${valijson_SOURCE_DIR}/include)

# --- Piano 1: nucleo (modello dati, regole, calcolo) -------------------------------
# Nessuna stampa, nessuna riga di comando, nessuna libreria esterna (lo controlla una prova).
add_library(kzero_nucleo STATIC
  src/nucleo/controlli.cpp
  src/nucleo/controlli_turni.cpp
  src/nucleo/dimostrazione.cpp
  src/nucleo/pacchetto.cpp
  src/nucleo/ricerca.cpp
  src/nucleo/tempo.cpp
  src/nucleo/turni.cpp
)
target_include_directories(kzero_nucleo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/nucleo)
target_compile_options(kzero_nucleo PRIVATE ${KZERO_WARNINGS})

# --- File incorporati nel programma: schemi di docs/schema e pacchetto dimostrativo esempi/demo ----
# Cosi' kzero controlla i dati sempre con gli schemi con cui e' stato compilato, da qualunque cartella.
file(GLOB KZERO_SCHEMI CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/schema/*.schema.json)
file(GLOB KZERO_DEMO CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/esempi/demo/*.json)
set(KZERO_RISORSE ${KZERO_SCHEMI} ${KZERO_DEMO})
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generato/risorse.cpp
  COMMAND ${CMAKE_COMMAND} -DSORGENTI=${CMAKE_CURRENT_SOURCE_DIR} -DUSCITA=${CMAKE_CURRENT_BINARY_DIR}/generato/risorse.cpp
          "-DFILE=${KZERO_RISORSE}" -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/incorpora.cmake
  DEPENDS ${KZERO_RISORSE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/incorpora.cmake
  COMMENT "Incorporo gli schemi e il pacchetto dimostrativo"
  VERBATIM
)

# --- Piano 2: API = riga di comando con uscita testo/JSON, schemi ed errori K-ERR ----
configure_file(src/api/versione.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generato/versione.hpp @ONLY)
add_library(kzero_api STATIC
  src/api/comandi.cpp
  src/api/comando_controlla.cpp
  src/api/comando_descrivi.cpp
  src/api/comando_dimostrazione.cpp
  src/api/comando_giornata.cpp
  src/api/comando_versione.cpp
  src/api/controlli_catalogo.cpp
  src/api/controllo.cpp
  src/api/errori.cpp
  src/api/esegui.cpp
  src/api/formato_dati.cpp
  src/api/lettore.cpp
  src/api/parametri.cpp
  src/api/riservatezza.cpp
  src/api/schemi_dati.cpp
  src/api/uscita.cpp
  src/api/validazione.cpp
  src/api/vista.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generato/risorse.cpp
)
target_include_directories(kzero_api
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/api
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generato
)
target_link_libraries(kzero_api
  PUBLIC kzero_nucleo nlohmann_json::nlohmann_json
  PRIVATE kzero_cli11 kzero_valijson)
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
