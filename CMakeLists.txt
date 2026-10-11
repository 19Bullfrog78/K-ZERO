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
  set(KZERO_WARNINGS /W4)
else()
  set(KZERO_WARNINGS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(kzero PRIVATE ${KZERO_WARNINGS})

# Prove automatiche delle regole del mestiere: ctest --test-dir build
enable_testing()
add_executable(test_regole tests/test_regole.cpp)
target_include_directories(test_regole PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_regole PRIVATE ${KZERO_WARNINGS})
foreach(prova
    giri_38_su_16
    assistente_occupa_un_posto
    carrozzina_toglie_posti
    senza_pedana_nessuna_carrozzina
    gruppi_diversi_ibrido
    studenti_centro_esterno_ibrido
    solo_studenti_scolastico
    solo_handicap
    solo_linea
    infanzia_assistente_obbligatorio
    pausa_breve_non_conta
    pausa_lunga_lontano_segnalata)
  add_test(NAME ${prova} COMMAND test_regole ${prova})
endforeach()

# OR-Tools verra' collegato nel passo successivo:
# find_package(ortools CONFIG REQUIRED)
# target_link_libraries(kzero PRIVATE ortools::ortools)
