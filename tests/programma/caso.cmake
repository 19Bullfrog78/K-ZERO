# K-Zero - prova sul programma: lancia kzero due volte con gli stessi argomenti e controlla
# - il codice di uscita;
# - che le due esecuzioni diano uscite identiche byte per byte (stesso input = stesso output);
# - risultato sull'uscita standard e niente sull'uscita di errore, oppure il contrario con il codice K-ERR atteso
#   (con RESOCONTO il risultato c'e' anche con l'uscita 1: e' il resoconto di kzero controlla sui dati sbagliati);
# - in formato json: JSON valido, con "schema" e "schema_versione"; per gli errori codice e uscita coerenti;
# - in formato testo: niente JSON, errori che iniziano con "Errore K-ERR-nnn";
# - se richiesto: uguaglianza con un file atteso, testi presenti o assenti, problemi K-CTR nel punto giusto.
# Con PACCHETTO_BASE prima prepara il pacchetto da controllare (pacchetto.cmake): "@PACCHETTO@" negli argomenti
# diventa la sua cartella.
# Uso: cmake -DKZERO=<programma> -DCASO=<file del caso, scritto da tests/CMakeLists.txt> -P caso.cmake
cmake_minimum_required(VERSION 3.25)

include("${CASO}")   # NOME, CARTELLA, USCITA, FORMATO, ARGOMENTI e, se servono, ERRORE, SCHEMA, ATTESO,
                     # CONTIENE, NON_CONTIENE, USCITA_SU, RESOCONTO, PROBLEMI, PACCHETTO_BASE, MODIFICA,
                     # INGRESSO, VARIABILI, PREPARA, PREPARA_INGRESSO (K-010)

function(fallisci messaggio)
  message(FATAL_ERROR "Prova ${NOME}: ${messaggio}")
endfunction()

file(MAKE_DIRECTORY "${CARTELLA}")
set(base "${CARTELLA}/${NOME}")
if(DEFINED PACCHETTO_BASE)
  set(PACCHETTO "${base}.pacchetto")
endif()

# @FF@ = il byte 0xFF, che non e' UTF-8: serve a provare i messaggi che ricopiano un argomento.
# @PACCHETTO@ = la copia del pacchetto di base; @NUOVA@ = una cartella nuova accanto (K-010: chiudi, esporta).
string(ASCII 255 byte_ff)
function(sostituisci_segnaposti elenco uscita)
  set(out "")
  foreach(a IN LISTS ${elenco})
    string(REPLACE "@FF@" "${byte_ff}" a "${a}")
    string(REPLACE "@PACCHETTO@" "${PACCHETTO}" a "${a}")
    string(REPLACE "@NUOVA@" "${PACCHETTO}.nuova" a "${a}")
    list(APPEND out "${a}")
  endforeach()
  set(${uscita} "${out}" PARENT_SCOPE)
endfunction()
sostituisci_segnaposti(ARGOMENTI argomenti)
sostituisci_segnaposti(PREPARA prepara)

# INGRESSO (K-010): le righe da dare sull'ingresso standard (le password di prova con --password-da-ingresso).
function(file_ingresso righe file)
  set(testo "")
  foreach(r IN LISTS ${righe})
    string(APPEND testo "${r}\n")
  endforeach()
  file(WRITE "${file}" "${testo}")
endfunction()
file_ingresso(INGRESSO "${base}.ingresso")
file_ingresso(PREPARA_INGRESSO "${base}.prepara.ingresso")

foreach(giro 1 2)
  # Ogni giro parte da una copia nuova del pacchetto: i comandi della cassaforte lo cambiano (registro, utenti).
  if(DEFINED PACCHETTO_BASE)
    include("${CMAKE_CURRENT_LIST_DIR}/pacchetto.cmake")
  endif()
  # PREPARA (K-010): un comando da lanciare prima, sulla copia (es. cambiare la data di conservazione); deve riuscire.
  if(prepara)
    execute_process(
      COMMAND "${KZERO}" ${prepara}
      INPUT_FILE "${base}.prepara.ingresso"
      OUTPUT_QUIET
      ERROR_VARIABLE errore_preparazione
      RESULT_VARIABLE codice_preparazione
    )
    if(NOT codice_preparazione EQUAL 0)
      fallisci("il comando di preparazione termina con ${codice_preparazione}: ${errore_preparazione}")
    endif()
  endif()
  if(DEFINED USCITA_SU)
    set(destinazione "${USCITA_SU}")
  else()
    set(destinazione "${base}.uscita.${giro}")
  endif()
  execute_process(
    COMMAND "${KZERO}" ${argomenti}
    INPUT_FILE "${base}.ingresso"
    OUTPUT_FILE "${destinazione}"
    ERROR_FILE "${base}.errori.${giro}"
    RESULT_VARIABLE codice_${giro}
  )
  # VARIABILI (K-010): l'ora del registro (orologio del PC) e il codice di una cassaforte nuova (caso) cambiano a ogni
  # giro, per scelta (docs/sicurezza.md): si confrontano e si cercano i testi su una copia in cui sono sostituiti. Tutto il
  # resto deve essere identico; lo schema si controlla sul documento vero.
  foreach(flusso uscita errori)
    if(flusso STREQUAL "uscita" AND DEFINED USCITA_SU)
      continue()
    endif()
    file(READ "${base}.${flusso}.${giro}" testo)
    if(VARIABILI)
      string(REGEX REPLACE "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z" "ORA" testo "${testo}")
      string(REGEX REPLACE "[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]" "CODICE-CASSAFORTE" testo "${testo}")
    endif()
    file(WRITE "${base}.${flusso}.${giro}.confronto" "${testo}")
  endforeach()
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
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${base}.${flusso}.1.confronto" "${base}.${flusso}.2.confronto"
    RESULT_VARIABLE diversi
  )
  if(diversi)
    fallisci("le due esecuzioni danno '${flusso}' diverse: stesso input deve dare stesso output")
  endif()
endforeach()

set(testo_uscita "")
if(NOT DEFINED USCITA_SU)
  file(READ "${base}.uscita.1.confronto" testo_uscita)
endif()
file(READ "${base}.errori.1.confronto" testo_errori)

if(USCITA EQUAL 0 OR RESOCONTO)
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
  if(NOT USCITA EQUAL 0 AND NOT RESOCONTO)
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
  if(NOT USCITA EQUAL 0 AND NOT RESOCONTO)
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

# Problemi attesi nel resoconto di kzero controlla: "K-CTR-nnn|file|percorso JSON" (percorso vuoto = tutto il file).
foreach(atteso IN LISTS PROBLEMI)
  string(REPLACE "|" ";" parti "${atteso}")
  list(GET parti 0 codice_atteso)
  list(GET parti 1 file_atteso)
  list(LENGTH parti n)
  set(puntatore_atteso "")
  if(n GREATER 2)
    list(GET parti 2 puntatore_atteso)
  endif()
  set(trovato FALSE)
  if(FORMATO STREQUAL "json")
    string(JSON quanti LENGTH "${documento}" problemi)
    if(quanti GREATER 0)
      math(EXPR ultimo "${quanti} - 1")
      foreach(i RANGE ${ultimo})
        string(JSON codice GET "${documento}" problemi ${i} codice)
        string(JSON file ERROR_VARIABLE senza GET "${documento}" problemi ${i} posizione file)
        string(JSON puntatore ERROR_VARIABLE senza_puntatore GET "${documento}" problemi ${i} posizione puntatore)
        if(senza)
          set(file "")
        endif()
        if(senza_puntatore)
          set(puntatore "")
        endif()
        if(codice STREQUAL codice_atteso AND file STREQUAL file_atteso AND puntatore STREQUAL puntatore_atteso)
          set(trovato TRUE)
        endif()
      endforeach()
    endif()
  elseif(file_atteso STREQUAL "")
    string(FIND "${documento}" " ${codice_atteso}: " posizione)   # problema dell'intero pacchetto: senza posizione
    if(NOT posizione EQUAL -1)
      set(trovato TRUE)
    endif()
  else()
    set(dove "${file_atteso}")
    if(NOT puntatore_atteso STREQUAL "")
      string(APPEND dove ", ${puntatore_atteso}")
    endif()
    foreach(fine ")" ", riga" "; il punto preciso")
      string(FIND "${documento}" " ${codice_atteso} (${dove}${fine}" posizione)
      if(NOT posizione EQUAL -1)
        set(trovato TRUE)
      endif()
    endforeach()
  endif()
  if(NOT trovato)
    fallisci("manca il problema ${codice_atteso} in ${file_atteso} ${puntatore_atteso}. Resoconto:\n${documento}")
  endif()
endforeach()
