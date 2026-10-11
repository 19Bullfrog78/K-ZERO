# K-Zero - prova sulla documentazione: quello che dice "kzero descrivi" deve coincidere con
# - docs/errori.md: una riga "| codice | uscita | descrizione |" per ogni errore, e nessuna in piu';
# - docs/schema: un file per ogni schema elencato, con il nome docs/schema/<nome>.v<versione>.schema.json,
#   e nessun file non elencato.
# Uso: cmake -DKZERO=<programma> -DSORGENTI=<cartella del repository> -P documentazione.cmake
cmake_minimum_required(VERSION 3.25)

execute_process(
  COMMAND "${KZERO}" descrivi --formato json
  OUTPUT_VARIABLE descrizione
  RESULT_VARIABLE risultato
)
if(risultato)
  message(FATAL_ERROR "kzero descrivi --formato json termina con ${risultato}")
endif()

# --- docs/errori.md ------------------------------------------------------------------
file(READ "${SORGENTI}/docs/errori.md" errori_md)
string(JSON quanti LENGTH "${descrizione}" errori)
string(REGEX MATCHALL "\n\\| K-ERR-[0-9][0-9][0-9] \\|" righe "${errori_md}")
list(LENGTH righe quante_righe)
if(NOT quante_righe EQUAL quanti)
  message(FATAL_ERROR "docs/errori.md ha ${quante_righe} righe di errori, kzero descrivi ne elenca ${quanti}")
endif()
math(EXPR ultimo "${quanti} - 1")
foreach(i RANGE ${ultimo})
  string(JSON codice GET "${descrizione}" errori ${i} codice)
  string(JSON uscita GET "${descrizione}" errori ${i} codice_uscita)
  string(JSON testo GET "${descrizione}" errori ${i} descrizione)
  string(REGEX MATCHALL "\n\\| ${codice} \\|" volte "${errori_md}")
  list(LENGTH volte quante)
  if(NOT quante EQUAL 1)
    message(FATAL_ERROR "docs/errori.md: ${codice} compare ${quante} volte, deve comparire una volta")
  endif()
  string(FIND "${errori_md}" "\n| ${codice} | ${uscita} | ${testo} |" posizione)
  if(posizione EQUAL -1)
    message(FATAL_ERROR "docs/errori.md: la riga di ${codice} deve iniziare con \"| ${codice} | ${uscita} | ${testo} |\"")
  endif()
endforeach()

# --- docs/schema ---------------------------------------------------------------------
string(JSON quanti LENGTH "${descrizione}" schemi)
math(EXPR ultimo "${quanti} - 1")
set(elencati "")
foreach(i RANGE ${ultimo})
  string(JSON nome GET "${descrizione}" schemi ${i} nome)
  string(JSON versione GET "${descrizione}" schemi ${i} versione)
  string(JSON file GET "${descrizione}" schemi ${i} file)
  if(NOT file STREQUAL "docs/schema/${nome}.v${versione}.schema.json")
    message(FATAL_ERROR "schema ${nome} versione ${versione}: file \"${file}\", atteso docs/schema/${nome}.v${versione}.schema.json")
  endif()
  if(NOT EXISTS "${SORGENTI}/${file}")
    message(FATAL_ERROR "manca il file dello schema ${file}")
  endif()
  list(APPEND elencati "${file}")
endforeach()
file(GLOB presenti RELATIVE "${SORGENTI}" "${SORGENTI}/docs/schema/*")
list(SORT presenti)
list(SORT elencati)
if(NOT presenti STREQUAL elencati)
  message(FATAL_ERROR "docs/schema contiene: ${presenti}\nkzero descrivi elenca: ${elencati}")
endif()
message(STATUS "docs/errori.md e docs/schema coincidono con kzero descrivi")
