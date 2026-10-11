# K-Zero - prova sull'architettura: il nucleo (src/nucleo) non stampa, non scrive file,
# non legge la riga di comando e non usa le librerie dell'API. Vedi docs/architettura.md.
# Uso: cmake -DSORGENTI=<cartella del repository> -P architettura.cmake
cmake_minimum_required(VERSION 3.25)

file(GLOB_RECURSE sorgenti "${SORGENTI}/src/nucleo/*")
if(NOT sorgenti)
  message(FATAL_ERROR "nessun file in src/nucleo")
endif()
set(vietati "<iostream>" "<cstdio>" "<stdio.h>" "<fstream>" "std::cout" "std::cerr" "printf" "CLI11" "nlohmann")
foreach(f IN LISTS sorgenti)
  file(READ "${f}" testo)
  foreach(v IN LISTS vietati)
    string(FIND "${testo}" "${v}" posizione)
    if(NOT posizione EQUAL -1)
      message(FATAL_ERROR "${f} contiene ${v}: il nucleo non stampa, non scrive file e non legge la riga di comando")
    endif()
  endforeach()
endforeach()
list(LENGTH sorgenti quanti)
message(STATUS "nucleo pulito: ${quanti} file controllati")
