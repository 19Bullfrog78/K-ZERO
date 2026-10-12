# K-Zero - prova sul programma: lancia kzero due volte con gli stessi argomenti e controlla
# - il codice di uscita;
# - che le due esecuzioni diano uscite identiche byte per byte (stesso input = stesso output);
# - risultato sull'uscita standard e niente sull'uscita di errore, oppure il contrario con il codice K-ERR atteso;
# - in formato json: JSON valido, con "schema" e "schema_versione"; per gli errori codice e uscita coerenti;
# - in formato testo: niente JSON, errori che iniziano con "Errore K-ERR-nnn";
# - se richiesto: uguaglianza con un file atteso, testi presenti o assenti.
# Uso: cmake -DKZERO=<programma> -DCASO=<file del caso, scritto da tests/CMakeLists.txt> -P caso.cmake
cmake_minimum_required(VERSION 3.25)

include("${CASO}")   # NOME, CARTELLA, USCITA, FORMATO, ARGOMENTI e, se servono, ERRORE, SCHEMA, ATTESO,
                     # CONTIENE, NON_CONTIENE, USCITA_SU

function(fallisci messaggio)
  message(FATAL_ERROR "Prova ${NOME}: ${messaggio}")
endfunction()

# @FF@ = il byte 0xFF, che non e' UTF-8: serve a provare i messaggi che ricopiano un argomento.
string(ASCII 255 byte_ff)
set(argomenti "")
foreach(a IN LISTS ARGOMENTI)
  string(REPLACE "@FF@" "${byte_ff}" a "${a}")
  list(APPEND argomenti "${a}")
endforeach()

file(MAKE_DIRECTORY "${CARTELLA}")
set(base "${CARTELLA}/${NOME}")
foreach(giro 1 2)
  if(DEFINED USCITA_SU)
    set(destinazione "${USCITA_SU}")
  else()
    set(destinazione "${base}.uscita.${giro}")
  endif()
  execute_process(
    COMMAND "${KZERO}" ${argomenti}
    OUTPUT_FILE "${destinazione}"
    ERROR_FILE "${base}.errori.${giro}"
    RESULT_VARIABLE codice_${giro}
  )
endforeach()

if(NOT "${codice_1}" STREQUAL "${USCITA}")
  file(READ "${base}.errori.1" dettaglio)
  fallisci("codice di uscita ${codice_1}, atteso ${USCITA}. Uscita di errore:\n${dettaglio}")
endif()
if(NOT "${codice_2}" STREQUAL "${codice_1}")
  fallisci("la seconda esecuzione termina con ${codice_2}, la prima con ${codice_1}")
endif()

set(flussi errori)
if(NOT DEFINED USCITA_SU)
  list(APPEND flussi uscita)
endif()
foreach(flusso IN LISTS flussi)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${base}.${flusso}.1" "${base}.${flusso}.2"
    RESULT_VARIABLE diversi
  )
  if(diversi)
    fallisci("le due esecuzioni danno '${flusso}' diverse: stesso input deve dare stesso output")
  endif()
endforeach()

set(testo_uscita "")
if(NOT DEFINED USCITA_SU)
  file(READ "${base}.uscita.1" testo_uscita)
endif()
file(READ "${base}.errori.1" testo_errori)

if(USCITA EQUAL 0)
  if(NOT testo_errori STREQUAL "")
    fallisci("riuscito, ma l'uscita di errore non e' vuota:\n${testo_errori}")
  endif()
  if(testo_uscita STREQUAL "")
    fallisci("riuscito, ma il risultato e' vuoto")
  endif()
  set(documento "${testo_uscita}")
  set(file_documento "${base}.uscita.1")
else()
  if(NOT testo_uscita STREQUAL "")
    fallisci("con un errore il risultato deve restare vuoto, invece contiene:\n${testo_uscita}")
  endif()
  if(testo_errori STREQUAL "")
    fallisci("errore senza messaggio")
  endif()
  set(documento "${testo_errori}")
  set(file_documento "${base}.errori.1")
endif()

if(FORMATO STREQUAL "json")
  string(JSON schema ERROR_VARIABLE problema GET "${documento}" schema)
  if(problema)
    fallisci("JSON non valido o senza \"schema\": ${problema}\n${documento}")
  endif()
  string(JSON versione ERROR_VARIABLE problema GET "${documento}" schema_versione)
  if(problema)
    fallisci("JSON senza \"schema_versione\": ${problema}")
  endif()
  if(NOT schema STREQUAL "${SCHEMA}")
    fallisci("schema \"${schema}\", atteso \"${SCHEMA}\"")
  endif()
  if(NOT USCITA EQUAL 0)
    string(JSON codice ERROR_VARIABLE problema GET "${documento}" errori 0 codice)
    if(problema OR NOT codice STREQUAL "${ERRORE}")
      fallisci("codice di errore \"${codice}\", atteso \"${ERRORE}\"")
    endif()
    string(JSON codice_uscita ERROR_VARIABLE problema GET "${documento}" codice_uscita)
    if(problema OR NOT codice_uscita EQUAL USCITA)
      fallisci("codice_uscita nel JSON \"${codice_uscita}\", atteso ${USCITA}")
    endif()
  endif()
  # Copia per la prova sugli schemi (schema.${NOME}).
  file(COPY_FILE "${file_documento}" "${base}.json")
else()
  string(SUBSTRING "${documento}" 0 1 primo)
  if(primo STREQUAL "{")
    fallisci("in formato testo l'uscita non deve essere JSON")
  endif()
  if(NOT USCITA EQUAL 0)
    string(FIND "${documento}" "Errore ${ERRORE}" posizione)
    if(NOT posizione EQUAL 0)
      fallisci("l'errore deve iniziare con \"Errore ${ERRORE}\", invece:\n${documento}")
    endif()
  endif()
endif()

if(DEFINED ATTESO)
  # --ignore-eol: su Windows l'uscita in modo testo va a capo con CR LF.
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol "${ATTESO}" "${base}.uscita.1"
    RESULT_VARIABLE diverso
  )
  if(diverso)
    fallisci("il risultato e' diverso da ${ATTESO}")
  endif()
endif()
foreach(t IN LISTS CONTIENE)
  string(FIND "${documento}" "${t}" posizione)
  if(posizione EQUAL -1)
    fallisci("manca il testo \"${t}\"")
  endif()
endforeach()
foreach(t IN LISTS NON_CONTIENE)
  string(FIND "${documento}" "${t}" posizione)
  if(NOT posizione EQUAL -1)
    fallisci("contiene il testo \"${t}\", che qui non deve comparire")
  endif()
endforeach()
