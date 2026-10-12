# Prove automatiche: ctest --test-dir build --output-on-failure
# Etichette: nucleo, api, programma, documentazione, architettura, schemi.
include(GoogleTest)

# --- Prove del nucleo (GoogleTest): ogni TEST si registra da solo --------------------
foreach(prova test_regole test_dimostrazione)
  add_executable(${prova} ${prova}.cpp)
  target_link_libraries(${prova} PRIVATE kzero_nucleo GTest::gtest_main)
  target_compile_options(${prova} PRIVATE ${KZERO_WARNINGS})
  gtest_discover_tests(${prova} PROPERTIES LABELS nucleo)
endforeach()

# --- Prove dell'API in memoria (GoogleTest) -----------------------------------------
add_executable(test_api test_api.cpp)
target_link_libraries(test_api PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_api PRIVATE ${KZERO_WARNINGS})
target_compile_definitions(test_api PRIVATE KZERO_CARTELLA_PROVE="${CMAKE_CURRENT_SOURCE_DIR}")
gtest_discover_tests(test_api PROPERTIES LABELS api)

# --- Strumento standard per confrontare le uscite JSON con gli schemi ---------------
# check-jsonschema, versioni e impronte fissate in tests/strumenti/check-jsonschema.txt.
find_program(KZERO_CHECK_JSONSCHEMA NAMES check-jsonschema)
option(KZERO_SCHEMI_OBBLIGATORI "Ferma la configurazione se manca check-jsonschema (controllo GitHub)" OFF)
if(NOT KZERO_CHECK_JSONSCHEMA)
  set(installa "come spiegato in CLAUDE.md (sezione Comandi), da tests/strumenti/check-jsonschema.txt")
  if(KZERO_SCHEMI_OBBLIGATORI)
    message(FATAL_ERROR "check-jsonschema non trovato. Installarlo ${installa}.")
  endif()
  message(WARNING "check-jsonschema non trovato: le prove sugli schemi (schema.*) non vengono registrate. "
                  "Installarlo ${installa}.")
endif()

# --- Prove sul programma: lanciano kzero (tests/programma/caso.cmake) ----------------
# kzero_prova(<nome> USCITA <codice> FORMATO testo|json [ERRORE K-ERR-nnn] [SCHEMA <nome>]
#             [ATTESO <file>] [USCITA_SU <file>] [CONTIENE <testi>] [NON_CONTIENE <testi>]
#             ARGOMENTI <argomenti>)
# Per ogni prova in json registra anche schema.<nome>: il documento rispetta lo schema che dichiara.
set(KZERO_USCITE_PROVE ${CMAKE_CURRENT_BINARY_DIR}/uscite)

function(kzero_prova nome)
  cmake_parse_arguments(P "" "USCITA;FORMATO;ERRORE;SCHEMA;ATTESO;USCITA_SU" "ARGOMENTI;CONTIENE;NON_CONTIENE" ${ARGN})
  set(testo "set(NOME [==[${nome}]==])\nset(CARTELLA [==[${KZERO_USCITE_PROVE}]==])\n")
  string(APPEND testo "set(USCITA ${P_USCITA})\nset(FORMATO ${P_FORMATO})\n")
  foreach(chiave ERRORE SCHEMA ATTESO USCITA_SU)
    if(DEFINED P_${chiave})
      string(APPEND testo "set(${chiave} [==[${P_${chiave}}]==])\n")
    endif()
  endforeach()
  foreach(elenco ARGOMENTI CONTIENE NON_CONTIENE)
    string(APPEND testo "set(${elenco}")
    foreach(valore IN LISTS P_${elenco})
      string(APPEND testo " [==[${valore}]==]")
    endforeach()
    string(APPEND testo ")\n")
  endforeach()
  set(caso ${CMAKE_CURRENT_BINARY_DIR}/casi/${nome}.cmake)
  file(WRITE ${caso} "${testo}")

  add_test(NAME programma.${nome}
           COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DCASO=${caso}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/caso.cmake)
  set_tests_properties(programma.${nome} PROPERTIES LABELS programma)
  if(P_FORMATO STREQUAL "json" AND KZERO_CHECK_JSONSCHEMA)
    set_tests_properties(programma.${nome} PROPERTIES FIXTURES_SETUP uscita.${nome})
    add_test(NAME schema.${nome}
             COMMAND ${CMAKE_COMMAND} -DVALIDATORE=${KZERO_CHECK_JSONSCHEMA} -DSORGENTI=${PROJECT_SOURCE_DIR}
                     -DDOCUMENTO=${KZERO_USCITE_PROVE}/${nome}.json
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/schema.cmake)
    set_tests_properties(schema.${nome} PROPERTIES FIXTURES_REQUIRED uscita.${nome} LABELS schemi)
  endif()
endfunction()

# Dati personali di prova: nel JSON di base non devono comparire.
set(DATI_PERSONALI "Marcena" "Livo, piazza" "via Centrale" "Autista A" "Autista B" "Deposito Rumo")

# Comandi riusciti: uscita 0, in testo e in JSON.
kzero_prova(versione_testo USCITA 0 FORMATO testo CONTIENE "kzero ${PROJECT_VERSION}\n" ARGOMENTI versione)
kzero_prova(versione_json USCITA 0 FORMATO json SCHEMA versione ARGOMENTI versione --formato json)
kzero_prova(descrivi_testo USCITA 0 FORMATO testo CONTIENE "K-ERR-001" "docs/schema/errore.v1.schema.json"
            ARGOMENTI descrivi)
kzero_prova(descrivi_json USCITA 0 FORMATO json SCHEMA descrivi ARGOMENTI descrivi --formato=json)
kzero_prova(dimostrazione_testo USCITA 0 FORMATO testo ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/dimostrazione.txt
            ARGOMENTI dimostrazione)
kzero_prova(dimostrazione_testo_con_interruttore USCITA 0 FORMATO testo
            ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/dimostrazione.txt
            ARGOMENTI dimostrazione --includi-dati-personali)
kzero_prova(dimostrazione_json USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": false" NON_CONTIENE ${DATI_PERSONALI}
            ARGOMENTI dimostrazione --formato json)
kzero_prova(dimostrazione_json_dati_personali USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": true" ${DATI_PERSONALI}
            ARGOMENTI dimostrazione --includi-dati-personali --formato json)

# Errori nei parametri: uscita 2 e codice K-ERR, in testo e in JSON.
kzero_prova(senza_comando USCITA 2 FORMATO testo ERRORE K-ERR-001 ARGOMENTI)
kzero_prova(senza_comando_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-001 ARGOMENTI --formato json)
kzero_prova(parametro_prima_del_comando_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-001
            ARGOMENTI --formato json versione)
kzero_prova(comando_sconosciuto USCITA 2 FORMATO testo ERRORE K-ERR-002 ARGOMENTI pianifica)
kzero_prova(comando_sconosciuto_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-002
            ARGOMENTI pianifica --formato json)
if(NOT WIN32)
  # Un argomento che non e' UTF-8 viene ricopiato nel messaggio: il JSON deve restare valido.
  kzero_prova(comando_non_utf8_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-002
              ARGOMENTI "pianifica@FF@" --formato json)
endif()
kzero_prova(parametro_sconosciuto USCITA 2 FORMATO testo ERRORE K-ERR-003 ARGOMENTI versione --colore)
kzero_prova(parametro_di_altro_comando_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-003
            ARGOMENTI versione --includi-dati-personali --formato json)
kzero_prova(argomento_non_previsto USCITA 2 FORMATO testo ERRORE K-ERR-004 ARGOMENTI versione extra)
kzero_prova(due_comandi_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-004
            ARGOMENTI descrivi versione --formato json)
kzero_prova(valore_mancante USCITA 2 FORMATO testo ERRORE K-ERR-005 ARGOMENTI versione --formato)
kzero_prova(valore_vuoto USCITA 2 FORMATO testo ERRORE K-ERR-005 ARGOMENTI versione --formato=)
kzero_prova(valore_non_ammesso USCITA 2 FORMATO testo ERRORE K-ERR-006 ARGOMENTI versione --formato xml)
kzero_prova(parametro_ripetuto_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-007
            ARGOMENTI versione --formato json --formato json)
kzero_prova(interruttore_con_valore_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-008
            ARGOMENTI dimostrazione --includi-dati-personali=no --formato json)

# Errore interno: il risultato non si puo' scrivere (uscita su un disco sempre pieno).
if(EXISTS /dev/full)
  kzero_prova(uscita_non_scrivibile_json USCITA 3 FORMATO json SCHEMA errore ERRORE K-ERR-010 USCITA_SU /dev/full
              ARGOMENTI versione --formato json)
endif()

# --- Documentazione e architettura --------------------------------------------------
add_test(NAME documentazione.errori_e_schemi
         COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/documentazione.cmake)
set_tests_properties(documentazione.errori_e_schemi PROPERTIES LABELS documentazione)
add_test(NAME architettura.nucleo_senza_stampa
         COMMAND ${CMAKE_COMMAND} -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/architettura.cmake)
set_tests_properties(architettura.nucleo_senza_stampa PROPERTIES LABELS architettura)

# --- Schemi: validi per JSON Schema draft-07; esempi conformi -----------------------
if(KZERO_CHECK_JSONSCHEMA)
  file(GLOB schemi CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/docs/schema/*.json)
  add_test(NAME schema.metaschema COMMAND ${KZERO_CHECK_JSONSCHEMA} --check-metaschema ${schemi})
  set_tests_properties(schema.metaschema PROPERTIES LABELS schemi)
  file(GLOB esempi CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/esempi/*.json)
  foreach(esempio IN LISTS esempi)
    get_filename_component(nome ${esempio} NAME_WE)
    add_test(NAME schema.esempio.${nome}
             COMMAND ${CMAKE_COMMAND} -DVALIDATORE=${KZERO_CHECK_JSONSCHEMA} -DSORGENTI=${PROJECT_SOURCE_DIR}
                     -DDOCUMENTO=${esempio} -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/schema.cmake)
    set_tests_properties(schema.esempio.${nome} PROPERTIES LABELS schemi)
  endforeach()
endif()
