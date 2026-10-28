# Prove automatiche: ctest --test-dir build --output-on-failure
# Etichette: nucleo, api, programma, documentazione, architettura, schemi, scala, sicurezza, fuzz.
include(GoogleTest)

# --- Prove del nucleo (GoogleTest): ogni TEST si registra da solo --------------------
# I dati delle prove vengono dal pacchetto demo (esempi/demo), letto con l'API come lo legge il programma:
# per questo le prove del nucleo che usano i dati sono collegate anche a kzero_api.
add_executable(test_tempo test_tempo.cpp)
target_link_libraries(test_tempo PRIVATE kzero_nucleo GTest::gtest_main)
target_compile_options(test_tempo PRIVATE ${KZERO_WARNINGS})
gtest_discover_tests(test_tempo PROPERTIES LABELS nucleo)
foreach(prova test_regole test_dimostrazione test_ricerca test_pacchetto test_controlli test_turni test_orari)
  add_executable(${prova} ${prova}.cpp)
  target_link_libraries(${prova} PRIVATE kzero_api GTest::gtest_main)
  target_compile_options(${prova} PRIVATE ${KZERO_WARNINGS})
  target_compile_definitions(${prova} PRIVATE KZERO_SORGENTI="${PROJECT_SOURCE_DIR}")
  gtest_discover_tests(${prova} PROPERTIES LABELS nucleo)
endforeach()

# --- Prove dell'API in memoria (GoogleTest) -----------------------------------------
add_executable(test_api test_api.cpp)
target_link_libraries(test_api PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_api PRIVATE ${KZERO_WARNINGS})
target_compile_definitions(test_api PRIVATE KZERO_CARTELLA_PROVE="${CMAKE_CURRENT_SOURCE_DIR}")
gtest_discover_tests(test_api PROPERTIES LABELS api)
add_executable(test_giornata test_giornata.cpp)
target_link_libraries(test_giornata PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_giornata PRIVATE ${KZERO_WARNINGS})
target_compile_definitions(test_giornata PRIVATE KZERO_SORGENTI="${PROJECT_SOURCE_DIR}")
gtest_discover_tests(test_giornata PROPERTIES LABELS api)
# K-010: cifratura (libsodium) e password, unione dei dati, registro, cassaforte. Una sola etichetta: gtest_discover_tests
# spezzerebbe un elenco di etichette ("api;sicurezza") in due proprieta' diverse.
add_executable(test_cripto test_cripto.cpp)
target_link_libraries(test_cripto PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_cripto PRIVATE ${KZERO_WARNINGS})
gtest_discover_tests(test_cripto PROPERTIES LABELS sicurezza)
add_executable(test_unione test_unione.cpp)
target_link_libraries(test_unione PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_unione PRIVATE ${KZERO_WARNINGS})
target_compile_definitions(test_unione PRIVATE KZERO_CARTELLA_PROVE="${CMAKE_CURRENT_SOURCE_DIR}")
gtest_discover_tests(test_unione PROPERTIES LABELS sicurezza)
add_executable(test_registro test_registro.cpp)
target_link_libraries(test_registro PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_registro PRIVATE ${KZERO_WARNINGS})
gtest_discover_tests(test_registro PROPERTIES LABELS sicurezza)
add_executable(test_cassaforte test_cassaforte.cpp)
target_link_libraries(test_cassaforte PRIVATE kzero_api GTest::gtest_main)
target_compile_options(test_cassaforte PRIVATE ${KZERO_WARNINGS})
target_compile_definitions(test_cassaforte PRIVATE KZERO_CARTELLA_PROVE="${CMAKE_CURRENT_SOURCE_DIR}")
gtest_discover_tests(test_cassaforte PROPERTIES LABELS sicurezza)

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
# K-010: piccolo strumento per cambiare un byte di un file binario (cassaforte, registro) nelle prove di manomissione.
add_executable(cambia_byte strumenti/cambia_byte.cpp)
target_compile_options(cambia_byte PRIVATE ${KZERO_WARNINGS})
# kzero_prova(<nome> USCITA <codice> FORMATO testo|json [ERRORE K-ERR-nnn] [SCHEMA <nome>]
#             [ATTESO <file>] [USCITA_SU <file>] [CONTIENE <testi>] [NON_CONTIENE <testi>]
#             [RESOCONTO] [PROBLEMI <K-CTR-nnn|file|percorso JSON>...]
#             [BASE <cartella> MODIFICA <operazione> <argomenti>...]
#             [INGRESSO <righe>...] [VARIABILI] [PREPARA <argomenti>... PREPARA_INGRESSO <righe>...]
#             ARGOMENTI <argomenti>)
# RESOCONTO: il risultato va sull'uscita standard anche con l'uscita 1 (kzero controlla su dati sbagliati).
# BASE e MODIFICA: il pacchetto da controllare e' una copia di BASE con una sola modifica (vedi
# tests/programma/pacchetto.cmake); negli ARGOMENTI la sua cartella si scrive @PACCHETTO@.
# Per ogni prova in json registra anche schema.<nome>: il documento rispetta lo schema che dichiara.
# K-010: INGRESSO = righe sull'ingresso standard (password di prova con --password-da-ingresso); VARIABILI = ora del registro
# e codice di una cassaforte nuova sostituiti prima dei confronti; PREPARA = un comando lanciato prima, sulla copia; senza
# MODIFICA la BASE si copia com'e'. Con BASE ogni giro parte da una copia nuova.
set(KZERO_USCITE_PROVE ${CMAKE_CURRENT_BINARY_DIR}/uscite)

function(kzero_prova nome)
  cmake_parse_arguments(P "RESOCONTO;VARIABILI" "USCITA;FORMATO;ERRORE;SCHEMA;ATTESO;USCITA_SU;BASE"
                        "ARGOMENTI;CONTIENE;NON_CONTIENE;PROBLEMI;MODIFICA;INGRESSO;PREPARA;PREPARA_INGRESSO" ${ARGN})
  set(testo "set(NOME [==[${nome}]==])\nset(CARTELLA [==[${KZERO_USCITE_PROVE}]==])\n")
  string(APPEND testo "set(USCITA ${P_USCITA})\nset(FORMATO ${P_FORMATO})\n")
  foreach(chiave ERRORE SCHEMA ATTESO USCITA_SU)
    if(DEFINED P_${chiave})
      string(APPEND testo "set(${chiave} [==[${P_${chiave}}]==])\n")
    endif()
  endforeach()
  if(P_RESOCONTO)
    string(APPEND testo "set(RESOCONTO TRUE)\n")
  endif()
  if(P_VARIABILI)
    string(APPEND testo "set(VARIABILI TRUE)\n")
  endif()
  if(DEFINED P_BASE)
    string(APPEND testo "set(PACCHETTO_BASE [==[${P_BASE}]==])\n")
  endif()
  foreach(elenco ARGOMENTI CONTIENE NON_CONTIENE PROBLEMI MODIFICA INGRESSO PREPARA PREPARA_INGRESSO)
    string(APPEND testo "set(${elenco}")
    foreach(valore IN LISTS P_${elenco})
      string(APPEND testo " [==[${valore}]==]")
    endforeach()
    string(APPEND testo ")\n")
  endforeach()
  set(caso ${CMAKE_CURRENT_BINARY_DIR}/casi/${nome}.cmake)
  file(WRITE ${caso} "${testo}")

  add_test(NAME programma.${nome}
           COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DCAMBIA_BYTE=$<TARGET_FILE:cambia_byte> -DCASO=${caso}
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

# Pacchetto rotto: copia del demo con UNA modifica; kzero controlla deve uscire con 1 e segnalare i problemi
# attesi nel punto giusto. kzero_rotto(<nome> FORMATO testo|json MODIFICA ... PROBLEMI ... [altro come kzero_prova])
set(KZERO_DEMO ${PROJECT_SOURCE_DIR}/esempi/demo)
function(kzero_rotto nome)
  cmake_parse_arguments(R "" "FORMATO" "" ${ARGN})
  set(argomenti controlla @PACCHETTO@)
  set(schema "")
  if(R_FORMATO STREQUAL "json")
    list(APPEND argomenti --formato json)
    set(schema SCHEMA controlla)
  endif()
  kzero_prova(${nome} USCITA 1 FORMATO ${R_FORMATO} ${schema} RESOCONTO BASE ${KZERO_DEMO} ${R_UNPARSED_ARGUMENTS}
              ARGOMENTI ${argomenti})
endfunction()

# Dati personali e particolari del pacchetto demo: di base non devono comparire nelle uscite.
# Dal K-010 il nome e la casa di chi va al centro diurno (luogo sensibile del demo) valgono come dati particolari: si
# vedono solo con tutti e due i parametri (DATI_PERSONALI_LEGATI); gli altri dati personali con il primo.
set(DATI_PERSONALI_LIBERI "Esposito Sara" "Gallo Pietro" "abitazione di un autista" "Spostata da Lagobello")
set(DATI_PERSONALI_LEGATI "Bianchi Luca" "Casa Bianchi" "Casa Neri")
set(DATI_PERSONALI ${DATI_PERSONALI_LIBERI} ${DATI_PERSONALI_LEGATI})
set(DATI_PARTICOLARI "Deambulatore" "\"disabili\"" "carrozzina, assistente" "carrozzina piegata" "\"ibrido\"")
# Le fermate pubbliche si vedono sempre.
set(FERMATE_PUBBLICHE "Pradello, Piazza" "Castelrovo, Alto" "Valfonda, Stazione FVV")

# Comandi riusciti: uscita 0, in testo e in JSON.
kzero_prova(versione_testo USCITA 0 FORMATO testo CONTIENE "kzero ${PROJECT_VERSION}\n" ARGOMENTI versione)
kzero_prova(versione_json USCITA 0 FORMATO json SCHEMA versione ARGOMENTI versione --formato json)
kzero_prova(descrivi_testo USCITA 0 FORMATO testo
            CONTIENE "K-ERR-001" "docs/schema/errore.v1.schema.json" "Uso: kzero controlla <cartella> [parametri]"
                     "K-CTR-001" "pacchetto.json" "docs/schema/corse.v1.schema.json"
            ARGOMENTI descrivi)
kzero_prova(descrivi_json USCITA 0 FORMATO json SCHEMA descrivi ARGOMENTI descrivi --formato=json)

# Dimostrazione sul pacchetto demo incorporato: di base senza dati personali e particolari.
kzero_prova(dimostrazione_testo USCITA 0 FORMATO testo ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/dimostrazione.txt
            CONTIENE ${FERMATE_PUBBLICHE} NON_CONTIENE ${DATI_PERSONALI} "Deambulatore"
            ARGOMENTI dimostrazione)
# Luoghi sensibili (K-010, regola nuova): con i dati personali e senza quelli particolari il nome e la casa di chi va al
# centro diurno (luogo sensibile del demo) restano codici; il centro si vede, mai "riservato" al posto di un luogo. Con
# i dati particolari si vede tutto.
kzero_prova(dimostrazione_testo_con_interruttore USCITA 0 FORMATO testo
            ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/dimostrazione_dati_personali.txt
            CONTIENE ${DATI_PERSONALI_LIBERI} ${FERMATE_PUBBLICHE} "passeggero P-D1  attivo  sale a PP-01, scende a L-CENTRO"
                     "gruppo G-DIS  tipo: riservato  scuola SC-CENTRO"
                     "   07:52  PP-01  salgono 1" "   08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3"
                     "   08:05  PP-03  salgono 1" "PR-01  ricovero esterno  Garage presso l'abitazione di un autista"
            NON_CONTIENE ${DATI_PERSONALI_LEGATI} "Deambulatore" "scende a riservato" "scuola riservato" "  riservato  scendono"
            ARGOMENTI dimostrazione --includi-dati-personali)
kzero_prova(dimostrazione_luogo_sensibile_con_tutti_i_dati USCITA 0 FORMATO testo
            CONTIENE "Bianchi Luca  attivo  sale a PP-01, scende a L-CENTRO" "gruppo G-DIS  tipo: disabili  scuola SC-CENTRO"
                     "   08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3"
            NON_CONTIENE "scende a riservato" "scuola riservato" "  riservato  scendono"
            ARGOMENTI dimostrazione --includi-dati-personali --includi-dati-particolari)
kzero_prova(dimostrazione_luogo_sensibile_senza_nomi USCITA 0 FORMATO testo
            CONTIENE "passeggero P-D1  attivo  sale a PP-01, scende a L-CENTRO" "scuola SC-CENTRO"
            NON_CONTIENE "Bianchi Luca" "scende a riservato"
            ARGOMENTI dimostrazione)
kzero_prova(dimostrazione_testo_dati_particolari USCITA 0 FORMATO testo
            CONTIENE "Deambulatore" "tipo: disabili" "categoria: ibrido" ${FERMATE_PUBBLICHE} NON_CONTIENE ${DATI_PERSONALI}
            ARGOMENTI dimostrazione --includi-dati-particolari)
kzero_prova(dimostrazione_json USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": false" "\"dati_particolari_inclusi\": false" ${FERMATE_PUBBLICHE}
            NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
            ARGOMENTI dimostrazione --formato json)
kzero_prova(dimostrazione_json_dati_personali USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": true" ${DATI_PERSONALI_LIBERI} ${FERMATE_PUBBLICHE} "\"discesa\": \"L-CENTRO\""
                     "\"scuola\": \"SC-CENTRO\"" "\"luogo\": \"L-CENTRO\"" "\"luogo\": \"PP-03\"" "\"nome\": \"P-D1\""
            NON_CONTIENE ${DATI_PARTICOLARI} ${DATI_PERSONALI_LEGATI} "\"discesa\": \"riservato\"" "\"luogo\": \"riservato\""
            ARGOMENTI dimostrazione --includi-dati-personali --formato json)
kzero_prova(dimostrazione_json_dati_particolari USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_particolari_inclusi\": true" ${DATI_PARTICOLARI} ${FERMATE_PUBBLICHE}
            NON_CONTIENE ${DATI_PERSONALI}
            ARGOMENTI dimostrazione --includi-dati-particolari --formato json)
kzero_prova(dimostrazione_json_tutti_i_dati USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI} "\"24:12\""
            ARGOMENTI dimostrazione --includi-dati-particolari --includi-dati-personali --formato json)

# kzero controlla sui pacchetti di esempio: nessun errore (uscita 0), solo gli avvisi voluti (dal K-008 anche due
# scadenze nel periodo, due turni scoperti per un fatto con le date e il mezzo di scorta al posto di quello abituale).
# Dal K-009 di base una posizione non entra in un dato personale: l'abilitazione che scade (/persone/2/abilitazioni/2)
# diventa la persona (/persone/2), l'assenza (assenze.json /assenze/2) la persona assente (/persone/1).
set(AVVISI_DEMO "K-CTR-035|luoghi.json|/luoghi/5" "K-CTR-043|luoghi.json|/luoghi/17/trasbordo" "K-CTR-036||"
    "K-CTR-050|mezzi.json|/mezzi/6/autorizzazioni_uso/0" "K-CTR-050|persone.json|/persone/2"
    "K-CTR-078|persone.json|/persone/2" "K-CTR-078|persone.json|/persone/1"
    "K-CTR-082|servizi_fissi.json|/servizi_fissi/2")
set(AVVISI_DEMO_COMPLETI "K-CTR-050|persone.json|/persone/2/abilitazioni/2" "K-CTR-078|persone.json|/persone/2/abilitazioni/2"
    "K-CTR-078|assenze.json|/assenze/2")
set(luoghi_demo 23)          # 18 luoghi pubblici e 5 privati
set(luoghi_demo-numeri 18)   # "solo numeri": nessun luogo privato
foreach(pacchetto demo demo-numeri)
  string(REPLACE "-" "_" n ${pacchetto})
  kzero_prova(controlla_${n}_testo USCITA 0 FORMATO testo
              # Il riepilogo in due pezzi: in CMake il punto e virgola separa gli elementi di un elenco.
              CONTIENE "Riepilogo: 2 aziende, ${luoghi_${pacchetto}} luoghi, 9 servizi, 26 corse, 10 mezzi, 12 persone, 18 turni"
                       " 0 errori, 31 avvisi."
              PROBLEMI ${AVVISI_DEMO} NON_CONTIENE "Errore K-CTR" "abilitazioni" "assenze.json" "Problemi sui dati particolari"
              ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/${pacchetto})
  kzero_prova(controlla_${n}_json USCITA 0 FORMATO json SCHEMA controlla
              CONTIENE "\"errori\": 0" "\"avvisi\": 31" "\"posizione_completa_con\": \"--includi-dati-personali\""
              PROBLEMI ${AVVISI_DEMO} NON_CONTIENE "abilitazioni" "assenze.json"
              ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/${pacchetto} --formato json)
endforeach()
kzero_prova(controlla_demo_dati_personali USCITA 0 FORMATO testo PROBLEMI ${AVVISI_DEMO_COMPLETI} NON_CONTIENE "il punto preciso"
            CONTIENE "Dati personali e particolari nel pacchetto: 257 valori personali e 37 particolari: luoghi.json 25 personali"
                     "assenze.json 9 personali e 0 particolari"
            ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/demo --includi-dati-personali)
# Quali dati personali e particolari ci sono nel pacchetto (es. se va perso): di base solo i totali.
kzero_prova(controlla_demo_dati_presenti USCITA 0 FORMATO testo
            CONTIENE "Dati personali e particolari nel pacchetto: 257 valori personali e 37 particolari (file per file: --includi-dati-personali)."
            NON_CONTIENE "assenze.json"
            ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/demo)
kzero_prova(controlla_demo_tutti_i_dati USCITA 0 FORMATO json SCHEMA controlla
            PROBLEMI ${AVVISI_DEMO_COMPLETI} NON_CONTIENE "posizione_completa_con"
            CONTIENE "\"dati_personali_inclusi\": true" "\"dati_particolari_inclusi\": true"
            ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/demo --includi-dati-personali --includi-dati-particolari
                      --formato json)

# I pacchetti scritti con le versioni precedenti dei file restano validi: copia congelata del demo del K-007
# (calendari, mezzi, servizi e regolamenti nella versione 1), con gli stessi avvisi di allora.
set(AVVISI_K007 "K-CTR-035|luoghi.json|/luoghi/5" "K-CTR-043|luoghi.json|/luoghi/17/trasbordo" "K-CTR-036||")
set(luoghi_k007_demo 23)
set(luoghi_k007_demo_numeri 18)
foreach(pacchetto demo demo-numeri)
  string(REPLACE "-" "_" n ${pacchetto})
  kzero_prova(controlla_k007_${n}_testo USCITA 0 FORMATO testo
              CONTIENE "Riepilogo: 2 aziende, ${luoghi_k007_${n}} luoghi, 7 servizi, 10 corse, 6 mezzi, 0 persone, 0 turni"
                       " 0 errori, 3 avvisi."
              PROBLEMI ${AVVISI_K007} NON_CONTIENE "Errore K-CTR"
              ARGOMENTI controlla ${CMAKE_CURRENT_SOURCE_DIR}/esempi/k007-${pacchetto})
  kzero_prova(controlla_k007_${n}_json USCITA 0 FORMATO json SCHEMA controlla
              CONTIENE "\"errori\": 0" "\"avvisi\": 3" PROBLEMI ${AVVISI_K007}
              ARGOMENTI controlla ${CMAKE_CURRENT_SOURCE_DIR}/esempi/k007-${pacchetto} --formato json)
endforeach()

# Errori del programma con kzero controlla: cartella mancante o inesistente.
kzero_prova(controlla_senza_cartella USCITA 2 FORMATO testo ERRORE K-ERR-011 ARGOMENTI controlla)
kzero_prova(controlla_senza_cartella_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-011
            ARGOMENTI controlla --formato json)
kzero_prova(controlla_cartella_inesistente USCITA 1 FORMATO testo ERRORE K-ERR-012
            ARGOMENTI controlla ${CMAKE_CURRENT_BINARY_DIR}/cartella-che-non-esiste)
kzero_prova(controlla_cartella_inesistente_json USCITA 1 FORMATO json SCHEMA errore ERRORE K-ERR-012
            ARGOMENTI controlla ${CMAKE_CURRENT_BINARY_DIR}/cartella-che-non-esiste --formato json)
kzero_prova(controlla_due_cartelle USCITA 2 FORMATO testo ERRORE K-ERR-004
            ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/demo ${PROJECT_SOURCE_DIR}/esempi/demo-numeri)

# --- Pacchetti rotti: un errore dell'elenco per pacchetto, ottenuto cambiando un solo valore del demo ---------
# Forma dei file.
kzero_rotto(rotto_manca_manifesto FORMATO testo MODIFICA ELIMINA pacchetto.json
            PROBLEMI "K-CTR-001|pacchetto.json" CONTIENE "Controlli sul contenuto non eseguiti")
kzero_rotto(rotto_json_non_valido FORMATO testo MODIFICA SOSTITUISCI mezzi.json "\"posti\": 19," "\"posti\": 19"
            PROBLEMI "K-CTR-003|mezzi.json")
kzero_rotto(rotto_campo_ripetuto FORMATO testo
            MODIFICA SOSTITUISCI mezzi.json "\"targa\": \"AA 102 AA\"," "\"targa\": \"AA 102 AA\", \"targa\": \"AA 999 ZZ\","
            PROBLEMI "K-CTR-004|mezzi.json|/mezzi/1/targa")
kzero_rotto(rotto_schema_sbagliato FORMATO testo MODIFICA IMPOSTA luoghi.json /schema "\"fermate\""
            PROBLEMI "K-CTR-005|luoghi.json|/schema")
kzero_rotto(rotto_versione_non_supportata FORMATO testo MODIFICA IMPOSTA corse.json /schema_versione 2
            PROBLEMI "K-CTR-006|corse.json|/schema_versione")
kzero_rotto(rotto_versione_non_supportata_json FORMATO json MODIFICA IMPOSTA corse.json /schema_versione 2
            PROBLEMI "K-CTR-006|corse.json|/schema_versione" CONTIENE "\"controlli_sul_contenuto\": false")
kzero_rotto(rotto_non_conforme_allo_schema FORMATO testo MODIFICA IMPOSTA mezzi.json /mezzi/0/categoria "\"M4\""
            PROBLEMI "K-CTR-007|mezzi.json|/mezzi/0/categoria" CONTIENE "Valori ammessi: \"M1\", \"M2\", \"M3\"")
kzero_rotto(rotto_non_conforme_allo_schema_json FORMATO json MODIFICA IMPOSTA mezzi.json /mezzi/0/categoria "\"M4\""
            PROBLEMI "K-CTR-007|mezzi.json|/mezzi/0/categoria")
kzero_rotto(rotto_numero_assurdo FORMATO testo MODIFICA IMPOSTA mezzi.json /mezzi/0/posti 1600
            PROBLEMI "K-CTR-007|mezzi.json|/mezzi/0/posti" CONTIENE "valore troppo grande: al massimo 1000")
kzero_rotto(rotto_testo_vuoto FORMATO testo MODIFICA IMPOSTA corse.json /corse/9/nome "\"\""
            PROBLEMI "K-CTR-007|corse.json|/corse/9/nome" CONTIENE "Non rispetta lo schema: testo vuoto. Il file"
            NON_CONTIENE "forma non valida" "..")
# Un file salvato come "ANSI" (la "i" accentata di "giovedi" in Latin-1): errore con riga e colonna, senza il testo.
kzero_rotto(rotto_file_in_ansi FORMATO testo
            MODIFICA SOSTITUISCI corse.json "\"nome\": \"Centro diurno, giovedi'\"" "\"nome\": \"Centro diurno, gioved@BYTE-EC@\""
            PROBLEMI "K-CTR-048|corse.json" CONTIENE "Il file non e' salvato in UTF-8: alla riga 282, colonna 37"
            "scegliendo la codifica UTF-8" NON_CONTIENE "gioved")
kzero_rotto(rotto_file_in_ansi_json FORMATO json
            MODIFICA SOSTITUISCI corse.json "\"nome\": \"Centro diurno, giovedi'\"" "\"nome\": \"Centro diurno, gioved@BYTE-EC@\""
            PROBLEMI "K-CTR-048|corse.json" CONTIENE "\"riga\": 282" "\"colonna\": 37" NON_CONTIENE "gioved")
# Contenuto.
kzero_rotto(rotto_codice_doppio FORMATO testo MODIFICA IMPOSTA corse.json /corse/2/codice "\"C-AM-1\""
            PROBLEMI "K-CTR-009|corse.json|/corse/2")
kzero_rotto(rotto_riferimento_inesistente FORMATO testo MODIFICA IMPOSTA corse.json /corse/0/passaggi/1/luogo "\"F-PRADELO\""
            PROBLEMI "K-CTR-010|corse.json|/corse/0/passaggi/1/luogo")
kzero_rotto(rotto_luogo_del_tipo_sbagliato FORMATO testo MODIFICA IMPOSTA scuole.json /scuole/0/luogo "\"F-PRADELLO\""
            PROBLEMI "K-CTR-011|scuole.json|/scuole/0/luogo")
kzero_rotto(rotto_calendario_rovesciato FORMATO testo MODIFICA IMPOSTA calendari.json /calendari/3/al "\"2026-08-31\""
            PROBLEMI "K-CTR-012|calendari.json|/calendari/3")
kzero_rotto(rotto_calendario_eccezione_fuori_periodo FORMATO testo
            MODIFICA IMPOSTA calendari.json /calendari/3/date_aggiunte/0 "\"2027-08-15\""
            PROBLEMI "K-CTR-013|calendari.json|/calendari/3/date_aggiunte/0")
kzero_rotto(rotto_calendario_data_contraddittoria FORMATO testo
            MODIFICA IMPOSTA calendari.json /calendari/3/date_aggiunte/0 "\"2027-06-02\""
            PROBLEMI "K-CTR-014|calendari.json|/calendari/3/date_aggiunte/0")
kzero_rotto(rotto_calendario_senza_giorni FORMATO testo MODIFICA IMPOSTA calendari.json /calendari/4/giorni "[]"
            PROBLEMI "K-CTR-015|calendari.json|/calendari/4")
kzero_rotto(rotto_calendario_circolare FORMATO testo MODIFICA IMPOSTA calendari.json /calendari/0/base "\"CAL-INFANZIA\""
            PROBLEMI "K-CTR-016|calendari.json|/calendari/0/base")
kzero_rotto(rotto_calendario_combinato_con_se_stesso FORMATO testo
            MODIFICA IMPOSTA calendari.json /calendari/4/aggiunge "{\"calendari\": [\"CAL-FERIALI\"]}"
            PROBLEMI "K-CTR-049|calendari.json|/calendari/4/aggiunge/calendari/0")
kzero_rotto(rotto_festivita_inesistente FORMATO json
            MODIFICA IMPOSTA calendari.json /calendari/4/toglie/festivita/0 "\"FEST-XX\""
            PROBLEMI "K-CTR-010|calendari.json|/calendari/4/toglie/festivita/0")
# K-008: norme del paese, elenchi delle aziende, uso dei mezzi.
kzero_rotto(rotto_abilitazione_inesistente_nelle_norme FORMATO testo
            MODIFICA IMPOSTA norme.json /norme/0/requisiti_guida/2/alternative/1/abilitazioni/0 "\"D2\""
            PROBLEMI "K-CTR-010|norme.json|/norme/0/requisiti_guida/2/alternative/1/abilitazioni/0"
            CONTIENE "Il codice D2 non esiste tra le abilitazioni delle norme N-IT")
kzero_rotto(rotto_uso_del_mezzo_inesistente FORMATO json MODIFICA IMPOSTA mezzi.json /mezzi/0/uso "\"NOLEGGIO\""
            PROBLEMI "K-CTR-010|mezzi.json|/mezzi/0/uso")
# Le competenze richieste da un servizio sono un dato particolare (possono far capire chi viaggia): dal K-009 di base
# il problema non si elenca, si conta soltanto (con i 4 K-CTR-071 delle persone che non hanno la competenza scritta
# male); con --includi-dati-particolari si vede tutto.
kzero_rotto(rotto_competenza_inesistente FORMATO testo MODIFICA IMPOSTA servizi.json /servizi/3/competenze_richieste/0 "\"PEDANE\""
            CONTIENE "Problemi sui dati particolari (salute), non elencati uno per uno: 5 errori e nessun avviso. Per vederli: --includi-dati-particolari."
            NON_CONTIENE "PEDANE" "K-CTR-010" "K-CTR-071" "competenze_richieste")
kzero_prova(rotto_competenza_inesistente_dati_particolari USCITA 1 FORMATO testo RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA servizi.json /servizi/3/competenze_richieste/0 "\"PEDANE\""
            PROBLEMI "K-CTR-010|servizi.json|/servizi/3/competenze_richieste/0"
            CONTIENE "Il codice PEDANE non esiste tra le competenze dell'azienda ALFA" NON_CONTIENE "Problemi sui dati particolari"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-particolari)
kzero_rotto(rotto_impostazioni_azienda_ripetute FORMATO testo MODIFICA IMPOSTA aziende.json /aziende/1/azienda "\"ALFA\""
            PROBLEMI "K-CTR-009|aziende.json|/aziende/1")
kzero_rotto(rotto_distrazione_rovesciata FORMATO testo
            MODIFICA IMPOSTA mezzi.json /mezzi/4/autorizzazioni_uso
                     "[{\"uso\": \"NCC\", \"ente\": \"Provincia\", \"riferimento\": \"n. 1\", \"dal\": \"2027-02-28\", \"al\": \"2026-03-01\"}]"
            PROBLEMI "K-CTR-012|mezzi.json|/mezzi/4/autorizzazioni_uso/0")
# K-008: persone, assenze, fermi.
kzero_rotto(rotto_persona_doppia FORMATO testo MODIFICA IMPOSTA persone.json /persone/1/codice "\"P-A\""
            PROBLEMI "K-CTR-009|persone.json|/persone/1")
kzero_rotto(rotto_abilitazione_della_persona_inesistente FORMATO testo
            MODIFICA IMPOSTA persone.json /persone/0/abilitazioni/1/tipo "\"D2\""
            PROBLEMI "K-CTR-010|persone.json|/persone/0"
            CONTIENE "Il codice scritto in un dato personale (riservato) non esiste tra le abilitazioni delle norme N-IT"
            NON_CONTIENE "D2" "Rossi Mario")
kzero_rotto(rotto_tipo_di_assenza_inesistente FORMATO json MODIFICA IMPOSTA assenze.json /assenze/0/tipo "\"MALATTIA\""
            PROBLEMI "K-CTR-010|persone.json|/persone/0" NON_CONTIENE "MALATTIA" "assenze")
kzero_prova(rotto_tipo_di_assenza_inesistente_dati_personali USCITA 1 FORMATO json SCHEMA controlla RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA assenze.json /assenze/0/tipo "\"MALATTIA\""
            PROBLEMI "K-CTR-010|assenze.json|/assenze/0/tipo" CONTIENE "MALATTIA"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --formato json)
kzero_rotto(rotto_tipo_di_fermo_inesistente FORMATO testo MODIFICA IMPOSTA fermi.json /fermi/0/tipo "\"OFFICINA\""
            PROBLEMI "K-CTR-010|fermi.json|/fermi/0/tipo" CONTIENE "Il codice OFFICINA non esiste tra i tipi di fermo dell'azienda ALFA")
kzero_rotto(rotto_assenza_rovesciata FORMATO testo MODIFICA IMPOSTA assenze.json /assenze/0/al "\"2026-10-21\""
            # Dal K-011 i giorni del 21 e del 22 ottobre compaiono negli avvisi degli orari di P-C (i giorni del piano, non
            # dell'assenza): si controlla che le date dell'assenza non compaiano nel messaggio dell'errore.
            PROBLEMI "K-CTR-012|persone.json|/persone/0"
            NON_CONTIENE "finisce (2026-10-21)" "cominciare (2026-10-2" "assenze")
# Caso reale: "16.08" scritto tra 12.05 e 12.19.
kzero_rotto(rotto_orario_indietro FORMATO testo MODIFICA IMPOSTA corse.json /corse/3/passaggi/2/orario "\"16:08\""
            PROBLEMI "K-CTR-017|corse.json|/corse/3/passaggi/2"
            CONTIENE "l'orario 16:08 non sta tra quello del passaggio precedente (12:05) e quello del successivo (12:19)")
kzero_rotto(rotto_orario_indietro_json FORMATO json MODIFICA IMPOSTA corse.json /corse/3/passaggi/2/orario "\"16:08\""
            PROBLEMI "K-CTR-017|corse.json|/corse/3/passaggi/2")
kzero_rotto(rotto_fermata_ambigua_nome FORMATO testo MODIFICA IMPOSTA luoghi.json /luoghi/4/nome "\"Pradello - piazza\""
            PROBLEMI "K-CTR-018|luoghi.json|/luoghi/4" CONTIENE "\"PRADELLO PIAZZA\"")
kzero_rotto(rotto_fermata_ambigua_codice_esterno FORMATO testo
            MODIFICA IMPOSTA luoghi.json /luoghi/9/codici_esterni/0/codice "\"5101\""
            PROBLEMI "K-CTR-018|luoghi.json|/luoghi/9")
kzero_rotto(rotto_solo_numeri_con_dati_personali FORMATO testo MODIFICA IMPOSTA pacchetto.json /livello "\"solo_numeri\""
            PROBLEMI "K-CTR-019|luoghi.json|/luoghi_privati/0" "K-CTR-019|servizi.json|/servizi/3/domanda/gruppi/0/passeggeri/0"
            NON_CONTIENE ${DATI_PERSONALI})
kzero_rotto(rotto_a_bordo_sotto_zero FORMATO testo MODIFICA IMPOSTA corse.json /corse/0/passaggi/3/persone/0/scendono 17
            PROBLEMI "K-CTR-020|corse.json|/corse/0/passaggi/3")
kzero_rotto(rotto_attesa_fuori_regolamento FORMATO testo
            MODIFICA IMPOSTA scuole.json /scuole/0/orari/0/attesa_concordata/prima_minuti 30
            PROBLEMI "K-CTR-021|scuole.json|/scuole/0/orari/0/attesa_concordata")
kzero_rotto(rotto_mezzo_massimo_sotto_il_minimo FORMATO testo
            MODIFICA IMPOSTA contratti.json /contratti/2/regole/posti_massimi 20
            PROBLEMI "K-CTR-022|contratti.json|/contratti/2/regole")
# --- Lettura blindata (K-009) ------------------------------------------------------------
# Si legge solo un file normale dentro la cartella del pacchetto, mai piu' di 256 MB; una cartella con il nome di un
# file del pacchetto e' un errore proprio. Nessun errore a catena: il file c'e' ma non si usa, e i controlli sul
# contenuto non partono. TIMEOUT: se il programma restasse in attesa (una pipe, /dev/zero), la prova fallisce subito.
kzero_rotto(rotto_cartella_al_posto_del_file FORMATO testo MODIFICA CARTELLA persone.json
            PROBLEMI "K-CTR-090|persone.json" CONTIENE "Controlli sul contenuto non eseguiti" " 1 errore, 0 avvisi."
            NON_CONTIENE "K-CTR-010" "K-CTR-008")
if(NOT WIN32)
  kzero_rotto(rotto_pipe_al_posto_del_file FORMATO testo MODIFICA PIPE persone.json
              PROBLEMI "K-CTR-088|persone.json" CONTIENE "Controlli sul contenuto non eseguiti" NON_CONTIENE "K-CTR-010")
  kzero_rotto(rotto_collegamento_a_dev_zero FORMATO json MODIFICA COLLEGA persone.json /dev/zero
              PROBLEMI "K-CTR-089|persone.json" CONTIENE "\"errori\": 1,")
  kzero_rotto(rotto_collegamento_fuori_dalla_cartella FORMATO testo MODIFICA COLLEGA persone.json @FUORI@
              PROBLEMI "K-CTR-089|persone.json" NON_CONTIENE "K-CTR-010")
  kzero_rotto(rotto_collegamento_che_non_porta_a_nulla FORMATO testo MODIFICA COLLEGA persone.json archivio/non-c-e.json
              PROBLEMI "K-CTR-002|persone.json" CONTIENE "non porta a nessun file")
  # Un collegamento a un file normale dentro la cartella si segue: il pacchetto e' quello del demo (piu' l'avviso
  # sulla sottocartella "archivio", che non fa parte del formato).
  kzero_prova(collegamento_dentro_la_cartella USCITA 0 FORMATO testo BASE ${KZERO_DEMO} MODIFICA COLLEGA persone.json @DENTRO@
              PROBLEMI "K-CTR-008|archivio" CONTIENE " 0 errori, 32 avvisi."
              ARGOMENTI controlla @PACCHETTO@)
  kzero_prova(giornata_con_una_pipe USCITA 1 FORMATO testo ERRORE K-ERR-018 BASE ${KZERO_DEMO} MODIFICA PIPE turni.json
              ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22)
  # Un file di 256 MB e un byte (zeri "sparsi": non occupano il disco): non si legge, nemmeno in parte.
  kzero_rotto(rotto_file_troppo_grande FORMATO testo MODIFICA ALLUNGA persone.json 268435457
              PROBLEMI "K-CTR-053|persone.json" CONTIENE "piu' grande di 256 MB" NON_CONTIENE "K-CTR-048" "K-CTR-003")
  foreach(prova rotto_file_troppo_grande rotto_pipe_al_posto_del_file rotto_collegamento_a_dev_zero rotto_collegamento_fuori_dalla_cartella
                rotto_collegamento_che_non_porta_a_nulla collegamento_dentro_la_cartella giornata_con_una_pipe)
    set_tests_properties(programma.${prova} PROPERTIES TIMEOUT 60)
  endforeach()
endif()

# --- kzero giornata (K-008) ----------------------------------------------------------
# Le tre giornate chieste dal mandato, confrontate con le uscite attese: una persona e un mezzo in un giovedi' (il
# giorno della sostituzione di P-A) e una persona in un giorno festivo (orario festivo della linea di Beta).
set(DATI_PERSONE "Verdi Carlo" "Rossi Mario" "Bianco Anna" "Russo Elena" "Galli Sara" "Via del Mulino" "FORMAZIONE" "FERIE")
set(GIORNATA_DEMO giornata ${KZERO_DEMO})
foreach(caso "persona_giovedi;--data;2026-10-22;--persona;P-C" "mezzo_giovedi;--data;2026-10-22;--mezzo;ALFA-05"
             "persona_festivo;--data;2026-12-08;--persona;P-B3")
  list(POP_FRONT caso nome_caso)
  kzero_prova(giornata_${nome_caso} USCITA 0 FORMATO testo ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/giornata_${nome_caso}.txt
              NON_CONTIENE ${DATI_PERSONE} ${DATI_PERSONALI} "PEDANA" "tipo disabili"
              ARGOMENTI ${GIORNATA_DEMO} ${caso})
  kzero_prova(giornata_${nome_caso}_json USCITA 0 FORMATO json SCHEMA giornata
              NON_CONTIENE ${DATI_PERSONE} ${DATI_PERSONALI} ${DATI_PARTICOLARI} "PEDANA"
              ARGOMENTI ${GIORNATA_DEMO} ${caso} --formato json)
endforeach()
# Tutti i turni di un giorno di scuola: di base senza nomi e senza dati sulla salute; con i due parametri si vedono.
kzero_prova(giornata_tutti USCITA 0 FORMATO testo
            CONTIENE "Tutti i turni del giorno" "T-B-ACCOMP" "A bordo del mezzo: BETA-01" "Totali: 13 turni (nessuno scoperto"
            NON_CONTIENE ${DATI_PERSONE} ${DATI_PERSONALI} "PEDANA" "tipo disabili"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22)
kzero_prova(giornata_tutti_i_dati USCITA 0 FORMATO testo
            CONTIENE "Persona: P-C Verdi Carlo" "Competenze richieste: PEDANA" "tipo disabili"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --includi-dati-personali --includi-dati-particolari)
kzero_prova(giornata_tutti_i_dati_json USCITA 0 FORMATO json SCHEMA giornata
            CONTIENE "\"dati_personali_inclusi\": true" "\"dati_particolari_inclusi\": true" "Verdi Carlo" "PEDANA"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --includi-dati-personali --includi-dati-particolari --formato json)
kzero_prova(giornata_demo_numeri USCITA 0 FORMATO json SCHEMA giornata CONTIENE "\"livello\": \"solo_numeri\""
            ARGOMENTI giornata ${PROJECT_SOURCE_DIR}/esempi/demo-numeri --data 2026-10-22 --formato json)

# Case dei passeggeri e luoghi sensibili (K-010, regola nuova). Con i dati personali e senza quelli particolari, la casa
# di chi va al centro diurno (L-CENTRO, luogo sensibile del demo) resta un codice in ogni vista e in ogni corsa; il centro
# si vede sempre (corse, attivita', inizio e fine), mai "riservato" al posto di un luogo. In testo e in JSON.
kzero_prova(giornata_mezzo_casa_e_centro USCITA 0 FORMATO testo
            ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/giornata_mezzo_dati_personali.txt
            CONTIENE "07:52  PP-01  salgono 1" "08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3"
                     "Trasferimento a vuoto (VUOTO), da L-CENTRO (Valfonda, Centro diurno Il Faro) a L-RIMESSA-ALFA"
            NON_CONTIENE ${DATI_PERSONALI_LEGATI} "riservato  " "da riservato"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --mezzo ALFA-03 --includi-dati-personali)
foreach(caso "mezzo;--mezzo;ALFA-03" "persona;--persona;P-C" "tutti")
  list(POP_FRONT caso vista)
  kzero_prova(giornata_${vista}_casa_e_centro_testo USCITA 0 FORMATO testo
              CONTIENE "PP-01  salgono 1" "08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3"
                       "da L-CENTRO (Valfonda, Centro diurno Il Faro) a L-RIMESSA-ALFA"
              NON_CONTIENE ${DATI_PERSONALI_LEGATI} "riservato  " "da riservato"
              ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 ${caso} --includi-dati-personali)
  kzero_prova(giornata_${vista}_casa_e_centro_json USCITA 0 FORMATO json SCHEMA giornata
              CONTIENE "\"luogo\": \"PP-01\"" "\"luogo\": \"L-CENTRO\"" "Valfonda, Centro diurno Il Faro"
              NON_CONTIENE ${DATI_PERSONALI_LEGATI} "\"luogo\": \"riservato\"" "\"codice\": \"riservato\""
              ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 ${caso} --includi-dati-personali --formato json)
  # Con i dati particolari si vede anche la casa; di base la casa e' un codice e il centro si vede.
  kzero_prova(giornata_${vista}_casa_e_centro_tutti_i_dati USCITA 0 FORMATO testo
              CONTIENE "PP-01  Casa Bianchi" "08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3"
                       "da L-CENTRO (Valfonda, Centro diurno Il Faro) a L-RIMESSA-ALFA"
              ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 ${caso} --includi-dati-personali --includi-dati-particolari)
  kzero_prova(giornata_${vista}_casa_e_centro_di_base_json USCITA 0 FORMATO json SCHEMA giornata
              CONTIENE "\"luogo\": \"L-CENTRO\"" "\"luogo\": \"PP-01\"" NON_CONTIENE "Casa Bianchi"
              ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 ${caso} --formato json)
endforeach()
# L'auto elettrica porta Neri al trasbordo per il centro: la sua casa resta un codice anche se questa corsa non arriva
# al centro (con la regola del K-009 si leggeva "Casa Neri"). Il garage presso la casa di un autista si vede.
kzero_prova(giornata_casa_legata_su_una_corsa_senza_centro USCITA 0 FORMATO testo
            CONTIENE "08:05  PP-03  salgono 1" "08:25  L-TRASB-PONTE  Ponte di Sorgenta, piazzale  consegna il trasbordo T-01"
                     "Comincia a PR-01 (Garage presso l'abitazione di un autista)"
            NON_CONTIENE "Casa Neri" "riservato  "
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --mezzo ALFA-04 --includi-dati-personali)
# Un luogo segnato sensibile si vede sempre (qui la rimessa, solo in questa prova): non lega nessuno, perche' nessun
# passeggero ci va.
kzero_prova(giornata_inizio_e_fine_in_un_luogo_sensibile USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA luoghi.json /luoghi/0/sensibile true
            CONTIENE "presa servizio a L-RIMESSA-ALFA" "Comincia a L-RIMESSA-ALFA" "07:40  L-RIMESSA-ALFA"
            NON_CONTIENE "a riservato" "riservato  " "Casa Bianchi"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --persona P-C --includi-dati-personali)
kzero_prova(giornata_inizio_e_fine_in_un_luogo_sensibile_json USCITA 0 FORMATO json SCHEMA giornata BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA luoghi.json /luoghi/0/sensibile true
            CONTIENE "\"codice\": \"L-RIMESSA-ALFA\"" NON_CONTIENE "\"codice\": \"riservato\"" "\"luogo\": \"riservato\"" "Casa Bianchi"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --persona P-C --includi-dati-personali --formato json)
# Un luogo sensibile in una giornata senza case di passeggeri si vede (la scuola media, segnata sensibile solo qui).
kzero_prova(giornata_luogo_sensibile_senza_case USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA luoghi.json /luoghi/11/sensibile true
            CONTIENE "Persona P-F" "L-MEDIE  Valfonda, Scuola media  scendono 8" NON_CONTIENE "riservato  " "PP-0"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --persona P-F --includi-dati-personali)
# Principio del punto 12: un fatto con le date su un turno ricorrente e' un avviso in kzero controlla, e in kzero
# giornata quel giorno il turno e' scoperto o senza mezzo, con il motivo. Lo stesso fatto su un'assegnazione del
# singolo giorno e' un errore: kzero giornata non mostra nulla.
kzero_prova(avviso_assenza_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/3
            PROBLEMI "K-CTR-078|persone.json|/persone/0" CONTIENE " 0 errori, 30 avvisi." NON_CONTIENE "assenze.json"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(giornata_assenza_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/3
            CONTIENE "Persona: nessuna - TURNO SCOPERTO: la persona prevista P-A non puo' farlo (dati personali: riservati)."
                     "Prevista: P-A, rotazione ROT-ALFA, passo 11"
            NON_CONTIENE "FERIE"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --persona P-A)
kzero_prova(avviso_fermo_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/0
            PROBLEMI "K-CTR-079|fermi.json|/fermi/0" CONTIENE " 0 errori, 32 avvisi."
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(giornata_fermo_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/0
            CONTIENE "Mezzo: nessuno - TURNO SENZA MEZZO: il mezzo ALFA-01 (AA 101 AA) e' in fermo (REVISIONE)."
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-19 --mezzo ALFA-01)
kzero_rotto(rotto_assegnazione_su_persona_assente FORMATO testo MODIFICA IMPOSTA assegnazioni.json /assegnazioni/2/persona "\"P-A\""
            PROBLEMI "K-CTR-067|assegnazioni.json|/assegnazioni/2" NON_CONTIENE "FERIE")
kzero_prova(giornata_pacchetto_con_errori USCITA 1 FORMATO testo ERRORE K-ERR-018 BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA assegnazioni.json /assegnazioni/2/persona "\"P-A\""
            CONTIENE "Il pacchetto ha 1 errore nei dati" "kzero controlla"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22)
kzero_prova(giornata_pacchetto_con_errori_json USCITA 1 FORMATO json SCHEMA errore ERRORE K-ERR-018 BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA assegnazioni.json /assegnazioni/2/persona "\"P-A\""
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --formato json)
# Errori dei parametri e della data.
kzero_prova(giornata_senza_data USCITA 2 FORMATO testo ERRORE K-ERR-013 CONTIENE "Manca il parametro --data"
            ARGOMENTI ${GIORNATA_DEMO})
kzero_prova(giornata_senza_data_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-013 ARGOMENTI ${GIORNATA_DEMO} --formato json)
kzero_prova(giornata_data_senza_valore USCITA 2 FORMATO testo ERRORE K-ERR-005 ARGOMENTI ${GIORNATA_DEMO} --data)
kzero_prova(giornata_data_inesistente USCITA 2 FORMATO testo ERRORE K-ERR-014 CONTIENE "'2026-02-30'"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-02-30)
kzero_prova(giornata_data_scritta_male USCITA 2 FORMATO testo ERRORE K-ERR-014 ARGOMENTI ${GIORNATA_DEMO} --data 22/10/2026)
kzero_prova(giornata_codice_scritto_male USCITA 2 FORMATO testo ERRORE K-ERR-014
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --persona "P A")
# Un valore troppo lungo si ricopia accorciato; un byte che non e' UTF-8 non rompe il JSON dell'errore.
kzero_prova(giornata_codice_troppo_lungo USCITA 2 FORMATO testo ERRORE K-ERR-014
            CONTIENE "'PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP...'"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22
                      --persona PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP)
kzero_prova(giornata_codice_non_utf8_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-014
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --persona P@FF@ --formato json)
kzero_prova(giornata_persona_e_mezzo USCITA 2 FORMATO testo ERRORE K-ERR-015
            CONTIENE "I parametri --mezzo e --persona non si possono scrivere insieme"
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --persona P-C --mezzo ALFA-05)
kzero_prova(giornata_data_fuori_periodo USCITA 2 FORMATO testo ERRORE K-ERR-016 CONTIENE "(dal 2026-09-01 al 2027-08-31)"
            ARGOMENTI ${GIORNATA_DEMO} --data 2027-09-01)
kzero_prova(giornata_data_fuori_periodo_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-016
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-08-31 --formato json)
kzero_prova(giornata_persona_inesistente USCITA 2 FORMATO testo ERRORE K-ERR-017
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --persona P-ZZ)
kzero_prova(giornata_mezzo_inesistente USCITA 2 FORMATO testo ERRORE K-ERR-017
            ARGOMENTI ${GIORNATA_DEMO} --data 2026-10-22 --mezzo ALFA-99)
kzero_prova(giornata_cartella_inesistente USCITA 1 FORMATO testo ERRORE K-ERR-012
            ARGOMENTI giornata ${CMAKE_CURRENT_BINARY_DIR}/cartella-che-non-esiste --data 2026-10-22)

# --- Orari di lavoro (K-011): kzero ore ---------------------------------------------------------------------------
set(ORE_K011 ${CMAKE_CURRENT_SOURCE_DIR}/esempi/k011-orari)
# Dati personali degli orari: il rapporto, il contratto collettivo, le ore del contratto, le leggi che valgono per il
# rapporto (es. la L. 138, il regime del lavoro a chiamata, il contratto collettivo inventato) e i nomi delle persone.
set(DATI_PERSONALI_ORE "Rossi Mario" "Verdi Carlo" "rapporto dipendente" "rapporto titolare" "previste 39 ore"
    "ESEMPIO-01" "R-CHIAMATA" "\"dipendente_tempo_pieno\"" "\"titolare\"")
# La L. 138 vale per il rapporto: negli orari di una persona e' riservata (in kzero controlla si vede solo dove parla
# dell'azienda, es. la chiave del rientro K-CTR-105).
set(DATI_PERSONALI_ORE_PERSONA ${DATI_PERSONALI_ORE} "R-L138")
# La settimana della gita di P-A: il Reg. 561/2006 vale per la gita della domenica e, per estensione, tutta la settimana
# (INL 61/2021); il riposo settimanale dopo la gita arriva oltre i 6 periodi di 24 ore se il 561 vale per i servizi
# scolastici (da confermare: un avviso). Di base il rapporto, il contratto, le ore e la L. 138 sono riservati.
kzero_prova(ore_settimana_della_gita USCITA 0 FORMATO testo
            CONTIENE "Orari di lavoro dal 2026-11-09 al 2026-11-15 - pacchetto DEMO-2026"
                     "Persona P-A, azienda ALFA, rapporto riservato, contratto collettivo riservato, ore previste riservate"
                     "domenica 2026-11-15: T-GITA (autista, giorno), dalle 07:40 alle 17:55"
                     "Regime R-561 per guida e riposi: vale con certezza per tutte le corse."
                     "vale con certezza da lunedi' 2026-11-09 a domenica 2026-11-15 per guida, riposi (estensione alla settimana, per una corsa del 2026-11-15)"
                     "4 regimi riservati: dipendono dal rapporto o dal contratto collettivo della persona (--includi-dati-personali)."
                     "Rientro tra C-LIN-545 e C-LIN-549: fermo 2 ore e 13 minuti a L-RIMESSA-ALFA" "Avviso K-CTR-118"
                     "Settimana dal lunedi' 2026-11-09: guida 11 ore e 2 minuti"
            NON_CONTIENE ${DATI_PERSONALI_ORE_PERSONA} ${DATI_PERSONALI} ${DATI_PARTICOLARI} "regolare\""
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-11-09 --al 2026-11-15 --persona P-A)
kzero_prova(ore_settimana_della_gita_json USCITA 0 FORMATO json SCHEMA ore
            CONTIENE "\"schema\": \"ore\"" "\"rapporto\": \"riservato\"" "\"codice\": \"riservato\"" "\"tipo\": \"settimana\""
            NON_CONTIENE ${DATI_PERSONALI_ORE_PERSONA} ${DATI_PERSONALI} ${DATI_PARTICOLARI}
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-11-09 --al 2026-11-15 --persona P-A --formato json)
kzero_prova(ore_settimana_della_gita_dati_personali USCITA 0 FORMATO testo
            CONTIENE "Persona P-A (Rossi Mario), azienda ALFA, rapporto dipendente tempo pieno, contratto collettivo ESEMPIO-01, ore previste 39 ore a settimana"
                     "Regime R-L138 per " "(previste 39 ore, differenza -" "R-L138 lavoro effettivo"
            NON_CONTIENE ${DATI_PARTICOLARI} "regimi riservati"
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-11-09 --al 2026-11-15 --persona P-A --includi-dati-personali)
kzero_prova(ore_settimana_della_gita_tutti_i_dati_json USCITA 0 FORMATO json SCHEMA ore
            CONTIENE "\"scolastico\"" "\"dati_particolari_inclusi\": true" "\"rapporto\": \"dipendente_tempo_pieno\""
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-11-09 --al 2026-11-15 --persona P-A --includi-dati-personali
                      --includi-dati-particolari --formato json)
# Un giorno: il titolare P-C che sostituisce dopo la reperibilita' della notte; P-A con i due rientri giustificati.
kzero_prova(ore_giorno_dopo_la_reperibilita USCITA 0 FORMATO testo
            CONTIENE "giovedi' 2026-10-22: T-DIS (autista, giorno), T-SECONDO (autista, giorno), dalle 07:25 alle 20:30"
                     "Riposo prima: 55 minuti dal 2026-10-22 06:30" "dopo la reperibilita', che qui non conta come riposo"
                     "Corse con il mezzo ALFA-03 (13 persone): C-DIS-1 (25,8 km); con il mezzo ALFA-05 (56 persone)"
                     "Rientro tra C-DIS-1 e C-LIN-549: fermo 7 ore e 45 minuti a L-RIMESSA-ALFA"
            NON_CONTIENE ${DATI_PERSONALI_ORE_PERSONA} ${DATI_PERSONALI} ${DATI_PARTICOLARI} "servizio disabili"
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-22 --persona P-C)
kzero_prova(ore_giorno_rientri_giustificati USCITA 0 FORMATO testo
            CONTIENE "martedi' 2026-10-20: T-PRIMO (autista, rotazione), dalle 07:15 alle 17:15 (ampiezza 10 ore)"
                     "Rientro tra C-LIN-537 e C-RM-1: fermo 2 ore e 34 minuti a L-RIMESSA-ALFA"
                     "27 minuti di guida a vuoto in piu' (il luogo dopo e' lo stesso), con il fattore 2 ne servono almeno 54 minuti; fermo minimo 30 minuti; 12,8 km: giustificato."
                     "Problemi degli orari: nessuno in questo periodo."
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-20 --persona P-A)
# Tutte le persone di un giorno, con la media (solo da vedere); il pacchetto "solo numeri".
kzero_prova(ore_tutti USCITA 0 FORMATO testo CONTIENE "Persona P-C," "Persona P-B1," "Media tra le "
            NON_CONTIENE ${DATI_PERSONALI_ORE_PERSONA} ${DATI_PERSONALI} ${DATI_PARTICOLARI}
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-22)
kzero_prova(ore_tutti_json USCITA 0 FORMATO json SCHEMA ore CONTIENE "\"media_tra_le_persone\": {" "\"persona\": null"
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-22 --formato json)
kzero_prova(ore_demo_numeri USCITA 0 FORMATO json SCHEMA ore CONTIENE "\"livello\": \"solo_numeri\""
            ARGOMENTI ore ${PROJECT_SOURCE_DIR}/esempi/demo-numeri --dal 2026-11-09 --al 2026-11-15 --formato json)
# Una persona che nel periodo non lavora: si mostra lo stesso, senza giornate.
kzero_prova(ore_persona_senza_turni USCITA 0 FORMATO testo
            CONTIENE "Persona P-A, azienda ALFA" "Nessuna giornata di lavoro nel periodo." "Totali del periodo: nessuna giornata"
            ARGOMENTI ore ${ORE_K011} --data 2026-11-05 --persona P-A)
# Errori dei parametri: il periodo manca, e' a meta', e' rovesciato, ha --data insieme a --dal; date fuori dal periodo del
# pacchetto; persona che non c'e'; pacchetto con errori nei dati (K-ERR-018).
kzero_prova(ore_senza_periodo USCITA 2 FORMATO testo ERRORE K-ERR-040 CONTIENE "Manca il giorno o il periodo da mostrare"
            ARGOMENTI ore ${KZERO_DEMO})
kzero_prova(ore_senza_periodo_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-040 ARGOMENTI ore ${KZERO_DEMO} --formato json)
kzero_prova(ore_periodo_a_meta USCITA 2 FORMATO testo ERRORE K-ERR-040 CONTIENE "manca --al"
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-10-19)
kzero_prova(ore_periodo_a_meta_dal USCITA 2 FORMATO testo ERRORE K-ERR-040 CONTIENE "manca --dal"
            ARGOMENTI ore ${KZERO_DEMO} --al 2026-10-25)
kzero_prova(ore_periodo_rovesciato USCITA 2 FORMATO testo ERRORE K-ERR-039
            CONTIENE "Il periodo finisce (--al 2026-10-19) prima di cominciare (--dal 2026-10-25)"
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-10-25 --al 2026-10-19)
kzero_prova(ore_periodo_rovesciato_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-039
            ARGOMENTI ore ${KZERO_DEMO} --dal 2026-10-25 --al 2026-10-19 --formato json)
kzero_prova(ore_data_e_periodo USCITA 2 FORMATO testo ERRORE K-ERR-015
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-22 --dal 2026-10-19 --al 2026-10-25)
kzero_prova(ore_data_scritta_male USCITA 2 FORMATO testo ERRORE K-ERR-014 ARGOMENTI ore ${KZERO_DEMO} --dal 19/10/2026 --al 2026-10-25)
kzero_prova(ore_data_fuori_periodo USCITA 2 FORMATO testo ERRORE K-ERR-016 CONTIENE "La data 2027-09-01" "(dal 2026-09-01 al 2027-08-31)"
            ARGOMENTI ore ${KZERO_DEMO} --dal 2027-08-25 --al 2027-09-01)
kzero_prova(ore_data_fuori_periodo_json USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-016
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-08-31 --formato json)
kzero_prova(ore_persona_inesistente USCITA 2 FORMATO testo ERRORE K-ERR-017 CONTIENE "La persona P-ZZZ non c'e'"
            ARGOMENTI ore ${KZERO_DEMO} --data 2026-10-22 --persona P-ZZZ)
kzero_prova(ore_pacchetto_con_errori USCITA 1 FORMATO testo ERRORE K-ERR-018 BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/0/azienda "\"ZETA\"" CONTIENE "nei dati: gli orari non si mostrano"
            ARGOMENTI ore @PACCHETTO@ --data 2026-10-22)
kzero_prova(ore_cartella_inesistente USCITA 1 FORMATO testo ERRORE K-ERR-012
            ARGOMENTI ore ${CMAKE_CURRENT_BINARY_DIR}/cartella-che-non-esiste --data 2026-10-22)
# Gli errori del piano (le regole sugli orari) non bloccano kzero ore ne' kzero giornata: servono proprio a correggerli.
kzero_prova(ore_errori_del_piano_non_bloccano USCITA 0 FORMATO testo
            CONTIENE "Controllo del pacchetto: 15 errori (15 del piano degli orari, che non bloccano), 89 avvisi"
                     "Errore K-CTR-110: La persona P-L138 supera una regola sugli orari (dati personali: riservati), il 2026-11-05."
            ARGOMENTI ore ${ORE_K011} --dal 2026-11-05 --al 2026-11-06 --persona P-L138)
kzero_prova(giornata_errori_del_piano_non_bloccano USCITA 0 FORMATO testo
            CONTIENE "15 errori (15 del piano degli orari, che non bloccano)" "T-L138-GIO"
            ARGOMENTI giornata ${ORE_K011} --data 2026-11-05)
kzero_prova(giornata_errori_del_piano_non_bloccano_json USCITA 0 FORMATO json SCHEMA giornata CONTIENE "\"errori_del_piano\": 15"
            ARGOMENTI giornata ${ORE_K011} --data 2026-11-05 --formato json)
# Il pacchetto di prova degli orari (dati inventati): un problema per ogni tipo di regola e di controllo nuovo.
kzero_prova(controlla_orari USCITA 1 FORMATO testo RESOCONTO
            CONTIENE "15 errori (15 del piano degli orari), 89 avvisi."
            PROBLEMI "K-CTR-102|corse.json|/corse/34" "K-CTR-104|norme.json|/norme/0/regimi/2/regole/2"
                     "K-CTR-105|aziende.json|/aziende/0/rientro" "K-CTR-108|turni.json|/turni/31"
                     "K-CTR-109|persone.json|/persone/19" "K-CTR-110|persone.json|/persone/13" "K-CTR-110|persone.json|/persone/20"
                     "K-CTR-111|persone.json|/persone/19" "K-CTR-112|persone.json|/persone/18" "K-CTR-113|persone.json|/persone/13"
                     "K-CTR-114|persone.json|/persone/24" "K-CTR-115|persone.json|/persone/13" "K-CTR-117|persone.json|/persone/14"
                     "K-CTR-119|persone.json|/persone/22" "K-CTR-122|persone.json|/persone/13" "K-CTR-123|persone.json|/persone/21"
                     "K-CTR-124|persone.json|/persone/12"
            NON_CONTIENE ${DATI_PERSONALI_ORE} "Linea Lunga" "Esempi Pausa"
            ARGOMENTI controlla ${ORE_K011})
kzero_prova(controlla_orari_json USCITA 1 FORMATO json SCHEMA controlla RESOCONTO
            CONTIENE "\"errori_del_piano\": 15" "\"natura\": \"piano\"" "\"natura\": \"dati\""
            ARGOMENTI controlla ${ORE_K011} --formato json)
kzero_prova(controlla_orari_dati_personali USCITA 1 FORMATO testo RESOCONTO
            CONTIENE "La persona P-L138 (Linea Lunga) supera la regola R-L138 L138-GIORNO (L. 14 febbraio 1958, n. 138, artt. 2 e 3): lavoro effettivo della giornata 11 ore e 35 minuti, massimo 10 ore, il 2026-11-05."
                     "La persona P-ORE (Ore Contratto) supera le ore massime del suo contratto: lavoro contato nella giornata 5 ore e 35 minuti, massimo 4 ore, il 2026-12-01."
                     "Straordinario: la persona P-TITOLARE (Titolare Prova) ha 9 ore e 35 minuti di lavoro effettivo, oltre il normale di 8 ore (massimo 10 ore) della regola R-L138 L138-GIORNO"
                     "va confermato se la legge vale per il rapporto di lavoro della persona"
            ARGOMENTI controlla ${ORE_K011} --includi-dati-personali)
# I tre esempi della pausa di Mauro (20/10), con la chiave del rientro di Alfa (30 minuti, fattore 2, preferenza).
kzero_prova(ore_tre_esempi_della_pausa USCITA 0 FORMATO testo
            CONTIENE "Rientro tra C-PAUSA-A1 e C-PAUSA-A2: fermo 30 minuti a L-RIMESSA-ALFA; 40 minuti di guida a vuoto in piu' (il luogo dopo e' lo stesso), con il fattore 2 ne servono almeno 1 ora e 20 minuti; fermo minimo 30 minuti; 20 km: non giustificato (fermo troppo corto per la guida in piu')."
                     "Rientro tra C-PAUSA-B1 e C-PAUSA-B2: fermo 15 minuti a L-RIMESSA-ALFA; 30 minuti di guida a vuoto in piu' (il luogo dopo e' lo stesso), con il fattore 2 ne servono almeno 1 ora; fermo minimo 30 minuti; 40 km: non giustificato (fermo sotto il minimo)."
                     "Rientro tra C-PAUSA-C1 e C-PAUSA-C2: fermo 2 ore e 16 minuti a L-RIMESSA-ALFA; 36 minuti di guida a vuoto in piu' (il luogo dopo e' lo stesso), con il fattore 2 ne servono almeno 1 ora e 12 minuti; fermo minimo 30 minuti; 24 km: giustificato."
                     "Avviso K-CTR-124"
            ARGOMENTI ore ${ORE_K011} --dal 2026-11-02 --al 2026-11-04 --persona P-PAUSE)
kzero_prova(ore_tre_esempi_della_pausa_json USCITA 0 FORMATO json SCHEMA ore
            CONTIENE "\"esito\": \"non_giustificato\"" "\"esito\": \"giustificato\"" "\"fattore_guida\": 2"
            ARGOMENTI ore ${ORE_K011} --dal 2026-11-02 --al 2026-11-04 --persona P-PAUSE --formato json)
# La regola dei 50 km del Reg. 561/2006: 52 km con il mezzo da 8 posti (9 persone con il conducente) no, da 9 posti si';
# con il mezzo da 17 posti di mercoledi' il 561 vale per tutta la settimana (estensione); senza km non si sa.
kzero_prova(ore_52_km_mezzo_da_8_posti USCITA 0 FORMATO testo
            CONTIENE "Corse con il mezzo V-8 (9 persone): C-52KM (52 km), C-52KM-R (52 km)." "Non valgono: R-561."
            ARGOMENTI ore ${ORE_K011} --data 2026-11-18 --persona P-561-8)
kzero_prova(ore_52_km_mezzo_da_9_posti USCITA 0 FORMATO testo
            CONTIENE "Corse con il mezzo V-9 (10 persone): C-52KM (52 km), C-52KM-R (52 km)."
                     "Regime R-561 per guida e riposi: vale con certezza per tutte le corse."
            ARGOMENTI ore ${ORE_K011} --data 2026-11-25 --persona P-561-9)
kzero_prova(ore_52_km_tutta_la_settimana USCITA 0 FORMATO testo
            CONTIENE "vale con certezza da lunedi' 2026-11-09 a domenica 2026-11-15 per guida, riposi (estensione alla settimana, per una corsa del 2026-11-11)"
                     "Errore K-CTR-113"
            ARGOMENTI ore ${ORE_K011} --dal 2026-11-09 --al 2026-11-15 --persona P-561-17)
# K-011 aggiunta 1: il limite dei 50 km (Reg. 561/2006 art. 3 a, "fino a 50 km"): una linea di esattamente 50 km con il
# mezzo da 17 posti e' esclusa, una di 50,1 km no.
kzero_prova(ore_50_km_esatti_esclusi USCITA 0 FORMATO testo
            CONTIENE "Corse con il mezzo V-17 (18 persone): C-50KM (50 km)." "Non valgono: R-561."
            NON_CONTIENE "Regime R-561"
            ARGOMENTI ore ${ORE_K011} --data 2026-11-16 --persona P-50KM)
kzero_prova(ore_50_virgola_1_km_561 USCITA 0 FORMATO testo
            CONTIENE "Corse con il mezzo V-17 (18 persone): C-50KM1 (50,1 km)."
                     "Regime R-561 per guida e riposi: vale con certezza per la corsa C-50KM1."
            ARGOMENTI ore ${ORE_K011} --data 2026-11-17 --persona P-50KM1)
kzero_prova(ore_senza_km USCITA 0 FORMATO testo
            CONTIENE "Corse con il mezzo V-17 (18 persone): C-SENZA-KM (km non noti)" "per i km del percorso"
            ARGOMENTI ore ${ORE_K011} --data 2026-11-19 --persona P-SENZA-KM)
# K-011 aggiunta 1 (caso dell'arbitro): ore massime del contratto contate con certezza solo quando sono certe. P-A con al
# massimo 2,1 ore al giorno; mercoledi' 14 ottobre fa solo T-DIS (C-DIS-1): L. 138, Reg. 561 e D.Lgs. 234 valgono tutti
# da confermare, quindi i conteggi possibili sono 2 ore e 10 (senza regime e L. 138) e 2 ore e 5 (D.Lgs. 234): nella
# lettura certa il piu' basso, sotto il limite. Un avviso con il motivo, non un errore. Con 2 ore tutti superano: errore.
kzero_prova(ore_contratto_errore_solo_se_certo USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/0/ore/0/massime/giorno 2.1
            CONTIENE "Avviso K-CTR-120: La persona P-A (Rossi Mario) potrebbe superare le ore massime del suo contratto: lavoro contato nella giornata 2 ore e 10 minuti, massimo 2 ore e 6 minuti"
                     "va confermato se la legge vale per il tipo di servizio della corsa"
                     "Lavoro: R-L138 lavoro effettivo 2 ore e 10 minuti; R-234 orario di lavoro 2 ore e 5 minuti; per il contratto 2 ore e 5 minuti"
            NON_CONTIENE "K-CTR-119"
            ARGOMENTI ore @PACCHETTO@ --data 2026-10-14 --persona P-A --includi-dati-personali --includi-dati-particolari)
kzero_prova(ore_contratto_errore_se_tutti_i_conteggi_superano USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/0/ore/0/massime/giorno 2
            CONTIENE "Errore K-CTR-119: La persona P-A (Rossi Mario) supera le ore massime del suo contratto: lavoro contato nella giornata 2 ore e 5 minuti, massimo 2 ore"
            ARGOMENTI ore @PACCHETTO@ --data 2026-10-14 --persona P-A --includi-dati-personali --includi-dati-particolari)
# Il titolare oltre le 8 ore della L. 138: la legge vale da confermare per il suo rapporto, quindi avvisi, mai errori.
kzero_prova(ore_titolare USCITA 0 FORMATO testo CONTIENE "Avviso K-CTR-112" "Avviso K-CTR-114" "rapporto titolare"
            NON_CONTIENE "Errore K-CTR"
            ARGOMENTI ore ${ORE_K011} --data 2026-11-20 --persona P-TITOLARE --includi-dati-personali)

# Turni (K-008), i due pacchetti rotti chiesti dal mandato. La reperibilita' notturna di P-C del mercoledi' finisce
# alle 07:30 invece che alle 06:30: il giovedi' della sostituzione P-C comincia il centro diurno alle 07:25.
kzero_rotto(rotto_persona_in_due_turni_a_cavallo_della_mezzanotte FORMATO testo
            MODIFICA IMPOSTA turni.json /turni/17/elementi/0/alle "\"31:30\""
            PROBLEMI "K-CTR-063|assegnazioni.json|/assegnazioni/2"
            CONTIENE "La persona P-C e' nel turno T-DIS (Centro diurno), che si sovrappone al turno T-REPERIBILE (Reperibilita' notturna) del giorno prima, a cavallo della mezzanotte, il 2026-10-22."
            NON_CONTIENE "Verdi Carlo")
kzero_rotto(rotto_persona_in_due_turni_a_cavallo_della_mezzanotte_json FORMATO json
            MODIFICA IMPOSTA turni.json /turni/17/elementi/0/alle "\"31:30\""
            PROBLEMI "K-CTR-063|assegnazioni.json|/assegnazioni/2")
# P-F senza la patente D: il minibus del suo turno (16 posti, lunghezza non scritta) la chiede. Un errore solo, con i
# 175 giorni di scuola raggruppati; quale abilitazione manca e' un dato personale.
kzero_rotto(rotto_autista_senza_abilitazione FORMATO testo MODIFICA TOGLI persone.json /persone/10/abilitazioni/1
            PROBLEMI "K-CTR-068|servizi_fissi.json|/servizi_fissi/4"
            CONTIENE "Nel turno T-SCUOLA-2 (Scuola 2) la persona P-F guida il mezzo ALFA-02 (AA 102 AA) senza un'abilitazione richiesta dalle norme (dati personali: riservati), in 175 giorni tra il 2026-09-14 e il 2027-06-11"
                     "1 errore, 31 avvisi."
            NON_CONTIENE "Moretti Luca" "serve una di")
kzero_rotto(rotto_autista_senza_abilitazione_json FORMATO json MODIFICA TOGLI persone.json /persone/10/abilitazioni/1
            PROBLEMI "K-CTR-068|servizi_fissi.json|/servizi_fissi/4")
# Senza la distrazione il mezzo di linea ALFA-05 non puo' fare la gita, che chiede l'uso noleggio. Sparisce anche
# l'avviso sulla distrazione che scade.
kzero_rotto(rotto_mezzo_senza_uso_richiesto FORMATO testo MODIFICA TOGLI mezzi.json /mezzi/6/autorizzazioni_uso/0
            PROBLEMI "K-CTR-069|turni.json|/turni/11/mezzo"
            CONTIENE "Nel turno T-GITA (Gita) il mezzo ALFA-05 (AA 105 AA) (uso LINEA) fa il servizio S-GITA (Gita della Pro Loco al lago di Lagobello), che chiede l'uso NCC, e non ha un'autorizzazione a quell'uso (distrazione), il 2026-11-15."
                     "1 errore, 30 avvisi.")
kzero_rotto(rotto_mezzo_senza_uso_richiesto_json FORMATO json MODIFICA TOGLI mezzi.json /mezzi/6/autorizzazioni_uso/0
            PROBLEMI "K-CTR-069|turni.json|/turni/11/mezzo")
kzero_rotto(rotto_trasbordo_non_autorizzato FORMATO testo MODIFICA TOGLI luoghi.json /luoghi/16/trasbordo
            PROBLEMI "K-CTR-023|corse.json|/corse/4/passaggi/2/trasbordi/0" "K-CTR-023|corse.json|/corse/5/passaggi/3/trasbordi/0")
kzero_rotto(rotto_trasbordo_senza_meta FORMATO testo MODIFICA IMPOSTA corse.json /corse/5/passaggi/3/trasbordi/0/codice "\"T-02\""
            PROBLEMI "K-CTR-024|corse.json|/corse/4/passaggi/2/trasbordi/0" "K-CTR-024|corse.json|/corse/5/passaggi/3/trasbordi/0")
# La corsa che riceve arriva alle 08:24: con il margine di 3 minuti doveva esserci entro le 08:22.
kzero_rotto(rotto_trasbordo_in_ritardo FORMATO testo MODIFICA IMPOSTA corse.json /corse/5/passaggi/3/orario "\"08:24\""
            PROBLEMI "K-CTR-025|corse.json|/corse/5/passaggi/3/trasbordi/0" CONTIENE "entro le 08:22")
kzero_rotto(rotto_trasbordo_in_ritardo_json FORMATO json MODIFICA IMPOSTA corse.json /corse/5/passaggi/3/orario "\"08:24\""
            PROBLEMI "K-CTR-025|corse.json|/corse/5/passaggi/3/trasbordi/0")
kzero_rotto(rotto_trasbordo_luoghi_diversi FORMATO testo MODIFICA IMPOSTA corse.json /corse/5/passaggi/3/luogo "\"L-TRASB-CROCE\""
            PROBLEMI "K-CTR-026|corse.json|/corse/5/passaggi/3/trasbordi/0")
kzero_rotto(rotto_trasbordo_riparte_prima FORMATO testo MODIFICA IMPOSTA corse.json /corse/5/passaggi/3/partenza "\"08:22\""
            PROBLEMI "K-CTR-027|corse.json|/corse/5/passaggi/3/trasbordi/0")
kzero_rotto(rotto_coincidenza_da_aspettare FORMATO testo MODIFICA IMPOSTA corse.json /corse/7/coincidenze/0/orario "\"07:55\""
            PROBLEMI "K-CTR-028|corse.json|/corse/7/coincidenze/0")
kzero_rotto(rotto_coincidenza_da_garantire FORMATO testo MODIFICA IMPOSTA corse.json /corse/8/coincidenze/0/orario "\"24:10\""
            PROBLEMI "K-CTR-029|corse.json|/corse/8/coincidenze/0")
kzero_rotto(rotto_coincidenza_fuori_corsa FORMATO testo MODIFICA IMPOSTA corse.json /corse/7/coincidenze/0/luogo "\"F-SORGENTA\""
            PROBLEMI "K-CTR-030|corse.json|/corse/7/coincidenze/0")
kzero_rotto(rotto_conteggi_diversi_dai_passeggeri FORMATO testo
            MODIFICA IMPOSTA servizi.json /servizi/4/domanda/gruppi/0/fermate/0/salgono 5
            PROBLEMI "K-CTR-031|servizi.json|/servizi/4/domanda/gruppi/0/fermate/0")
kzero_rotto(rotto_lotto_con_altro_contratto FORMATO testo MODIFICA IMPOSTA gruppi_servizi.json /gruppi_servizi/0/servizi/2 "\"S-ASILO\""
            PROBLEMI "K-CTR-032|gruppi_servizi.json|/gruppi_servizi/0/servizi/2")
kzero_rotto(rotto_servizio_in_due_lotti FORMATO testo
            MODIFICA IMPOSTA gruppi_servizi.json /gruppi_servizi/1
                     "{\"codice\": \"LOTTO-3\", \"nome\": \"Lotto 3\", \"contratto\": \"CT-SCU\", \"servizi\": [\"S-DIS\"]}"
            PROBLEMI "K-CTR-033|gruppi_servizi.json|/gruppi_servizi/1/servizi/0")
kzero_rotto(rotto_gruppo_di_altro_servizio FORMATO testo MODIFICA IMPOSTA corse.json /corse/2/servizi/0 "\"S-ASILO\""
            PROBLEMI "K-CTR-034|corse.json|/corse/2/passaggi/1/persone/0")

# Lo stesso controllo con due regolamenti diversi da' esiti diversi: l'attesa di 20 minuti della scuola media
# va bene con R-TN (massimo 25) e non con R-INV (massimo 15); senza R-INV le superiori non hanno durata massima.
kzero_rotto(due_regolamenti_medie_con_r_inv FORMATO testo MODIFICA IMPOSTA contratti.json /contratti/0/regolamento "\"R-INV\""
            PROBLEMI "K-CTR-021|scuole.json|/scuole/0/orari/0/attesa_concordata"
            CONTIENE "(minuti concordati: 20; massimo: 15)" "(minuti concordati: 15; massimo: 10)")
kzero_prova(due_regolamenti_linee_con_r_tn USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA contratti.json /contratti/2/regolamento "\"R-TN\""
            PROBLEMI "K-CTR-037|servizi.json|/servizi/2/domanda/gruppi/0" CONTIENE "0 errori, 32 avvisi."
            ARGOMENTI controlla @PACCHETTO@)

# Gli avvisi non bloccano: uscita 0. Es. domanda 8 a una fermata, corse 7.
kzero_prova(avviso_domanda_diversa_dalle_corse USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA servizi.json /servizi/0/domanda/gruppi/0/fermate/2/salgono 8
            PROBLEMI "K-CTR-042|servizi.json|/servizi/0/domanda/gruppi/0/fermate/2"
            CONTIENE "secondo la domanda (8) non corrispondono a quelle caricate dalle corse del calendario CAL-SCUOLA-LV"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(avviso_mezzo_senza_costi USCITA 0 FORMATO json SCHEMA controlla BASE ${KZERO_DEMO}
            MODIFICA TOGLI mezzi.json /mezzi/0/costo_al_km PROBLEMI "K-CTR-044|mezzi.json|/mezzi/0"
            ARGOMENTI controlla @PACCHETTO@ --formato json)
kzero_prova(avviso_distrazione_che_scade USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA mezzi.json /mezzi/4/autorizzazioni_uso
                     "[{\"uso\": \"NCC\", \"ente\": \"Provincia\", \"riferimento\": \"n. 1\", \"dal\": \"2026-03-01\", \"al\": \"2027-02-28\"}]"
            PROBLEMI "K-CTR-050|mezzi.json|/mezzi/4/autorizzazioni_uso/0" CONTIENE "scade il 2027-02-28"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(avviso_mezzo_senza_uso USCITA 0 FORMATO json SCHEMA controlla BASE ${KZERO_DEMO}
            MODIFICA TOGLI mezzi.json /mezzi/5/uso PROBLEMI "K-CTR-051|mezzi.json|/mezzi/5"
            ARGOMENTI controlla @PACCHETTO@ --formato json)
# Dal K-011 senza le norme non vale nessuna legge sugli orari: il lavoro di P-A si conta senza regime (guida, altre mansioni e
# disponibilita' sul posto) e la domenica della gita supera le 10 ore al giorno del suo contratto (K-CTR-119, errore del
# piano, uscita 1). Prima: uscita 0.
kzero_prova(avviso_azienda_senza_norme USCITA 1 FORMATO testo RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA TOGLI aziende.json /aziende/0/norme PROBLEMI "K-CTR-052|pacchetto.json|/aziende/0" "K-CTR-119|persone.json|/persone/0"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(avviso_abilitazione_che_scade USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/2/abilitazioni/2/scadenza "\"2027-03-31\""
            PROBLEMI "K-CTR-050|persone.json|/persone/2"
            CONTIENE "Un'abilitazione o un'autorizzazione della persona P-C scade dentro il periodo del pacchetto"
            # Dal K-011 il 2027-03-31 (un mercoledi') compare come ultimo giorno di lavoro di P-C negli avvisi degli orari, che
            # mostrano i giorni del piano: si controlla che la scadenza non compaia come scadenza.
            NON_CONTIENE "CQC" "scade il 2027-03-31" "Verdi Carlo"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(avviso_abilitazione_che_scade_dati_personali USCITA 0 FORMATO json SCHEMA controlla BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/2/abilitazioni/2/scadenza "\"2027-03-31\""
            PROBLEMI "K-CTR-050|persone.json|/persone/2/abilitazioni/2"
            CONTIENE "L'abilitazione CQC-P della persona P-C (Verdi Carlo) scade il 2027-03-31"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --formato json)
kzero_prova(avviso_file_non_riconosciuto USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA AGGIUNGI fermate.json "{}" PROBLEMI "K-CTR-008|fermate.json"
            ARGOMENTI controlla @PACCHETTO@)

# Classi dei dati nei messaggi di kzero controlla: di base il nome di una casa e il valore di un dato particolare
# non compaiono; con il parametro giusto si'.
kzero_rotto(classi_trasbordo_in_una_casa FORMATO testo MODIFICA IMPOSTA corse.json /corse/4/passaggi/2/luogo "\"PP-01\""
            PROBLEMI "K-CTR-023|corse.json|/corse/4/passaggi/2/trasbordi/0" CONTIENE "avviene in PP-01,"
            NON_CONTIENE "Casa Bianchi")
# PP-01 e' la casa di chi va al centro diurno: dal K-010 il nome si vede solo con anche i dati particolari.
kzero_prova(classi_trasbordo_in_una_casa_dati_personali USCITA 1 FORMATO json SCHEMA controlla RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA corse.json /corse/4/passaggi/2/luogo "\"PP-01\""
            PROBLEMI "K-CTR-023|corse.json|/corse/4/passaggi/2/trasbordi/0" CONTIENE "avviene in PP-01," NON_CONTIENE "Casa Bianchi"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --formato json)
kzero_prova(classi_trasbordo_in_una_casa_tutti_i_dati USCITA 1 FORMATO json SCHEMA controlla RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA corse.json /corse/4/passaggi/2/luogo "\"PP-01\""
            PROBLEMI "K-CTR-023|corse.json|/corse/4/passaggi/2/trasbordi/0" CONTIENE "PP-01 (Casa Bianchi)"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --includi-dati-particolari --formato json)
# Dal K-009 un problema che rivela un dato particolare (carrozzine, assistente, tipo "disabili") di base non si elenca:
# una riga sola, senza codice ne' posizione, con quanti errori e avvisi sono; il codice di uscita non cambia.
kzero_rotto(classi_carrozzine FORMATO testo MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/fermate/0/carrozzine 0
            CONTIENE "Problemi sui dati particolari (salute), non elencati uno per uno: 1 errore e nessun avviso." " 1 errore, 31 avvisi."
            NON_CONTIENE "carrozzin" "K-CTR-031" "/servizi/3/domanda")
kzero_prova(classi_carrozzine_dati_particolari USCITA 1 FORMATO testo RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/fermate/0/carrozzine 0
            PROBLEMI "K-CTR-031|servizi.json|/servizi/3/domanda/gruppi/0/fermate/0" CONTIENE "le persone sedute sulla carrozzina non corrispondono"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-particolari)
kzero_rotto(classi_assistente FORMATO testo
            MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/assistente_richiesto false
            CONTIENE "Problemi sui dati particolari (salute), non elencati uno per uno: 1 errore e nessun avviso."
            NON_CONTIENE "assistente" "Bianchi Luca" "K-CTR-047" "P-D1" "passeggeri")
kzero_prova(classi_assistente_dati_particolari USCITA 1 FORMATO json SCHEMA controlla RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/assistente_richiesto false
            PROBLEMI "K-CTR-047|servizi.json|/servizi/3/domanda/gruppi/0/passeggeri/0"
            CONTIENE "ha bisogno dell'assistente a bordo, ma il gruppo G-DIS" NON_CONTIENE "Bianchi Luca"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-particolari --formato json)
kzero_rotto(classi_valore_particolare_nello_schema FORMATO json
            MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/tipo "\"disabili_gravi\""
            CONTIENE "\"problemi_riservati\": {\n    \"errori\": 1," NON_CONTIENE "disabili_gravi" "K-CTR-007" "/servizi/3")
kzero_prova(classi_valore_particolare_nello_schema_mostrato USCITA 1 FORMATO testo RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/tipo "\"disabili_gravi\""
            PROBLEMI "K-CTR-007|servizi.json|/servizi/3/domanda/gruppi/0/tipo" CONTIENE "\"disabili_gravi\""
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-particolari)

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

# --- Cassaforte dei dati personali (K-010) ---------------------------------------------------------------------------
# Copia congelata della versione 1 della cassaforte: il demo chiuso con il programma del K-010 (parametri di rilascio), con
# tre utenti. Le password di prova sono scritte qui (dati inventati): TITOLARE ha i dati personali e particolari, UFFICIO i
# soli personali, SALUTE i soli particolari. Le prove lavorano sempre su una copia (BASE), mai sulla cartella del repository.
set(KZERO_CASSAFORTE ${CMAKE_CURRENT_SOURCE_DIR}/esempi/k010-cassaforte)
set(PW_TITOLARE "congelata per sempre nelle prove")
set(PW_UFFICIO "ufficio legge solo i dati personali")
set(PW_SALUTE "salute legge solo i dati particolari")
# kzero_cassaforte(<nome> <utente> USCITA ... ARGOMENTI <comando> ... ): kzero_prova su una copia della cassaforte, con la
# password dell'utente sull'ingresso standard e --password-da-ingresso in fondo agli argomenti.
function(kzero_cassaforte nome utente)
  cmake_parse_arguments(C "" "" "ARGOMENTI;INGRESSO_IN_PIU" ${ARGN})
  kzero_prova(${nome} BASE ${KZERO_CASSAFORTE} INGRESSO "${PW_${utente}}" ${C_INGRESSO_IN_PIU} ${C_UNPARSED_ARGUMENTS}
              ARGOMENTI ${C_ARGOMENTI} --utente ${utente} --password-da-ingresso)
endfunction()

# Versione 1 leggibile, permessi in ogni uscita: controlla per i tre tipi di utente, in testo e in JSON.
kzero_cassaforte(cassaforte_controlla_titolare TITOLARE USCITA 0 FORMATO testo PROBLEMI ${AVVISI_DEMO}
                 CONTIENE "Cassaforte aperta dall'utente TITOLARE (dati personali e particolari); conservazione dei dati fino al 2099-12-31."
                          # Dal K-011 la copia congelata (aziende, norme e persone versione 1) ha anche gli avvisi degli orari: le
                          # categorie di tempo non dette (2), i dipendenti senza nessuna legge sugli orari nei dati (8) e il
                          # contratto di P-A forse superato per le categorie presunte (5). Prima: 8 avvisi.
                          "248 valori personali e 37 particolari, nella cassaforte" " 0 errori, 24 avvisi."
                 NON_CONTIENE "Non verificato" "Bianchi" "Deambulatore"
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_controlla_titolare_json TITOLARE USCITA 0 FORMATO json SCHEMA controlla PROBLEMI ${AVVISI_DEMO}
                 CONTIENE "\"utente\": \"TITOLARE\"" "\"non_verificato\": []" "\"conservazione_scaduta\": false"
                 ARGOMENTI controlla @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_controlla_titolare_tutti_i_dati TITOLARE USCITA 0 FORMATO testo PROBLEMI ${AVVISI_DEMO_COMPLETI}
                 CONTIENE "248 valori personali e 37 particolari, nella cassaforte: luoghi.json 25 personali"
                 ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --includi-dati-particolari)
kzero_cassaforte(cassaforte_controlla_ufficio UFFICIO USCITA 0 FORMATO testo
                 CONTIENE "Cassaforte aperta dall'utente UFFICIO (dati personali)"
                          "Non verificato (servono i dati particolari, che l'utente non puo' aprire)" "K-CTR-068"
                 NON_CONTIENE "servono i dati personali" ${DATI_PERSONALI} ${DATI_PARTICOLARI}
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_controlla_ufficio_json UFFICIO USCITA 0 FORMATO json SCHEMA controlla
                 CONTIENE "\"servono\": \"particolari\"" "\"utente\": \"UFFICIO\"" NON_CONTIENE "\"servono\": \"personali\""
                 ARGOMENTI controlla @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_controlla_ufficio_dati_personali UFFICIO USCITA 0 FORMATO testo
                 CONTIENE "nella cassaforte: luoghi.json" "persone.json 161 personali e 0 particolari"
                 PROBLEMI "K-CTR-050|persone.json|/persone/2/abilitazioni/2"
                 NON_CONTIENE ${DATI_PERSONALI_LEGATI} ${DATI_PARTICOLARI} "K-CTR-078 ("
                 ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali)
kzero_cassaforte(cassaforte_controlla_salute SALUTE USCITA 0 FORMATO testo
                 CONTIENE "Cassaforte aperta dall'utente SALUTE (dati particolari)"
                          "Non verificato (servono i dati personali, che l'utente non puo' aprire)"
                 NON_CONTIENE "servono i dati particolari" ${DATI_PERSONALI_LIBERI}
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_controlla_salute_json SALUTE USCITA 0 FORMATO json SCHEMA controlla
                 CONTIENE "\"servono\": \"personali\"" NON_CONTIENE "\"servono\": \"particolari\""
                 ARGOMENTI controlla @PACCHETTO@ --formato json --includi-dati-particolari)
# Parametri oltre i permessi: un errore chiaro, non un'uscita filtrata in silenzio.
kzero_cassaforte(cassaforte_ufficio_dati_particolari UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 CONTIENE "--includi-dati-particolari: l'utente UFFICIO non ha il permesso dei dati particolari"
                 ARGOMENTI controlla @PACCHETTO@ --includi-dati-particolari)
kzero_cassaforte(cassaforte_ufficio_dati_particolari_json UFFICIO USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-023
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --includi-dati-particolari --formato json)
kzero_cassaforte(cassaforte_salute_dati_personali SALUTE USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --includi-dati-personali)
# giornata nelle tre viste (tutti, persona, mezzo), per i tre tipi di utente, in testo e in JSON.
foreach(utente TITOLARE UFFICIO SALUTE)
  string(TOLOWER ${utente} u)
  if(utente STREQUAL "TITOLARE")
    set(nota_verifica NON_CONTIENE "Non verificato")
  else()
    set(nota_verifica CONTIENE "Non verificato (servono i dati")
  endif()
  foreach(vista tutti persona mezzo)
    set(scelta "")
    if(vista STREQUAL "persona")
      set(scelta --persona P-C)
    elseif(vista STREQUAL "mezzo")
      set(scelta --mezzo ALFA-03)
    endif()
    kzero_cassaforte(cassaforte_giornata_${u}_${vista} ${utente} USCITA 0 FORMATO testo ${nota_verifica}
                     NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
                     ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 ${scelta})
    kzero_cassaforte(cassaforte_giornata_${u}_${vista}_json ${utente} USCITA 0 FORMATO json SCHEMA giornata
                     CONTIENE "\"utente\": \"${utente}\"" NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
                     ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 ${scelta} --formato json)
  endforeach()
endforeach()
# kzero ore (K-011) per i tre tipi di utente, sulla copia congelata (versione 1: categorie dei tempi presunte). Chi non puo'
# aprire una classe legge "non verificato" (leggi, lavoro, rientri, problemi), mai un risultato fatto su dati mancanti.
kzero_cassaforte(cassaforte_ore_titolare TITOLARE USCITA 0 FORMATO testo
                 CONTIENE "Cassaforte aperta dall'utente TITOLARE (dati personali e particolari)." "Persona P-A, azienda ALFA"
                          "categoria presunta per"
                 NON_CONTIENE "Non verificato" "non verificati" ${DATI_PERSONALI} ${DATI_PARTICOLARI} ${DATI_PERSONALI_ORE_PERSONA}
                 ARGOMENTI ore @PACCHETTO@ --dal 2026-10-19 --al 2026-10-25)
kzero_cassaforte(cassaforte_ore_titolare_tutti_i_dati TITOLARE USCITA 0 FORMATO testo
                 # Le norme della versione 1 non hanno regimi: nessuna legge sugli orari, mai "regolare".
                 CONTIENE "Persona P-A (Rossi Mario), azienda ALFA, rapporto dipendente tempo pieno"
                          "Nessuna legge sugli orari nei dati per questo caso: gli orari di questo giorno non si giudicano (non vuol dire \"regolare\")."
                 ARGOMENTI ore @PACCHETTO@ --dal 2026-10-19 --al 2026-10-25 --includi-dati-personali --includi-dati-particolari)
foreach(utente UFFICIO SALUTE)
  string(TOLOWER ${utente} u)
  if(utente STREQUAL "UFFICIO")
    set(manca particolari)
  else()
    set(manca personali)
  endif()
  kzero_cassaforte(cassaforte_ore_${u} ${utente} USCITA 0 FORMATO testo
                   CONTIENE "Non verificato (servono i dati ${manca}):"
                            "Leggi, lavoro e rientri: non verificati (servono insieme i dati personali e quelli particolari)."
                   NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI} ${DATI_PERSONALI_ORE_PERSONA} "Problemi degli orari"
                   ARGOMENTI ore @PACCHETTO@ --dal 2026-10-19 --al 2026-10-25)
  kzero_cassaforte(cassaforte_ore_${u}_json ${utente} USCITA 0 FORMATO json SCHEMA ore
                   CONTIENE "\"servono\": \"${manca}\"" "\"regimi\": null" "\"problemi\": null" "\"utente\": \"${utente}\""
                   NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI} ${DATI_PERSONALI_ORE_PERSONA}
                   ARGOMENTI ore @PACCHETTO@ --dal 2026-10-19 --al 2026-10-25 --formato json)
endforeach()
kzero_cassaforte(cassaforte_ore_ufficio_dati_personali UFFICIO USCITA 0 FORMATO testo
                 CONTIENE "Persona P-A (Rossi Mario), azienda ALFA, rapporto dipendente tempo pieno" "non verificati"
                 NON_CONTIENE ${DATI_PARTICOLARI} "Regime R-L138"
                 ARGOMENTI ore @PACCHETTO@ --dal 2026-10-19 --al 2026-10-25 --includi-dati-personali)
kzero_cassaforte(cassaforte_ore_ufficio_dati_particolari UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI ore @PACCHETTO@ --data 2026-10-22 --includi-dati-particolari)
kzero_cassaforte(cassaforte_ore_salute_dati_personali SALUTE USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-023
                 ARGOMENTI ore @PACCHETTO@ --data 2026-10-22 --includi-dati-personali --formato json)
# LegameSensibile segue i permessi: chi ha i soli dati personali non legge mai il nome o la casa di chi va al centro, ne'
# senza ne' con --includi-dati-personali; il centro si vede sempre.
kzero_cassaforte(cassaforte_giornata_mezzo_ufficio_dati_personali UFFICIO USCITA 0 FORMATO testo
                 CONTIENE "08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3" "07:52  PP-01  salgono 1"
                          "Persona: P-C Verdi Carlo"
                 NON_CONTIENE ${DATI_PERSONALI_LEGATI} ${DATI_PARTICOLARI} "riservato  scendono"
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --mezzo ALFA-03 --includi-dati-personali)
kzero_cassaforte(cassaforte_giornata_mezzo_ufficio_senza_parametri UFFICIO USCITA 0 FORMATO testo
                 CONTIENE "08:52  L-CENTRO  Valfonda, Centro diurno Il Faro  scendono 3" "07:52  PP-01  salgono 1"
                 NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --mezzo ALFA-03)
kzero_cassaforte(cassaforte_giornata_mezzo_titolare_tutti_i_dati TITOLARE USCITA 0 FORMATO testo
                 CONTIENE "07:52  PP-01  Casa Bianchi" "Valfonda, Centro diurno Il Faro"
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --mezzo ALFA-03 --includi-dati-personali --includi-dati-particolari)

# Comandi della cassaforte.
kzero_cassaforte(cassaforte_cassaforte TITOLARE USCITA 0 FORMATO testo
                 CONTIENE "Utenti: TITOLARE (dati personali e particolari), UFFICIO (dati personali), SALUTE (dati particolari)."
                          "Valori: 248 personali e 37 particolari (mai mostrati qui)." "conservazione fino al 2099-12-31"
                 ARGOMENTI cassaforte @PACCHETTO@)
kzero_cassaforte(cassaforte_cassaforte_json UFFICIO USCITA 0 FORMATO json SCHEMA cassaforte
                 CONTIENE "\"comando\": \"cassaforte\"" "\"generazione\": 1"
                 ARGOMENTI cassaforte @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_registro TITOLARE USCITA 0 FORMATO testo VARIABILI
                 CONTIENE "1. ORA  TITOLARE  chiudi  aperte: dati personali e particolari; mostrate: nessuna; pacchetto chiuso"
                          "2. ORA  TITOLARE  aggiungi-utente" "4. ORA  TITOLARE  registro"
                 ARGOMENTI registro @PACCHETTO@)
kzero_cassaforte(cassaforte_registro_json SALUTE USCITA 0 FORMATO json SCHEMA registro VARIABILI
                 CONTIENE "\"catena_controllata\": true" "\"classi_aperte\": [\n        \"particolari\"\n      ]"
                 ARGOMENTI registro @PACCHETTO@ --formato json)
# Il registro dopo una giornata: utente, comando, classi aperte e mostrate, nessun dato.
kzero_cassaforte(cassaforte_registro_dopo_giornata UFFICIO USCITA 0 FORMATO testo VARIABILI
                 PREPARA giornata @PACCHETTO@ --data 2026-10-22 --mezzo ALFA-03 --includi-dati-personali --utente UFFICIO
                         --password-da-ingresso
                 PREPARA_INGRESSO "${PW_UFFICIO}"
                 CONTIENE "4. ORA  UFFICIO  giornata  aperte: dati personali; mostrate: dati personali; giornata del 2026-10-22, mezzo ALFA-03"
                 NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
                 ARGOMENTI registro @PACCHETTO@)

# Chiusura del demo: identica alla riapertura; manifesto 1, cartella che c'e' gia', password corta.
kzero_prova(cassaforte_chiudi BASE ${KZERO_DEMO} USCITA 0 FORMATO testo VARIABILI INGRESSO "una frase lunga per chiudere"
            CONTIENE "riaperto e controllato, i dati sono identici all'originale" "Valori: 257 personali e 37 particolari"
                     "va cancellata a mano"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
kzero_prova(cassaforte_chiudi_json BASE ${KZERO_DEMO} USCITA 0 FORMATO json SCHEMA cassaforte VARIABILI
            INGRESSO "una frase lunga per chiudere"
            CONTIENE "\"comando\": \"chiudi\"" "\"voci_registro\": 1"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso --formato json)
kzero_prova(cassaforte_chiudi_manifesto_1 BASE ${CMAKE_CURRENT_SOURCE_DIR}/esempi/k007-demo USCITA 1 FORMATO testo ERRORE K-ERR-034
            INGRESSO "una frase lunga per chiudere"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
kzero_prova(cassaforte_chiudi_cartella_esistente BASE ${KZERO_DEMO} USCITA 2 FORMATO testo ERRORE K-ERR-033
            INGRESSO "una frase lunga per chiudere"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @PACCHETTO@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
kzero_prova(cassaforte_chiudi_password_corta BASE ${KZERO_DEMO} USCITA 2 FORMATO testo ERRORE K-ERR-020
            INGRESSO "corta" CONTIENE "almeno 15"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
kzero_prova(cassaforte_chiudi_password_comune BASE ${KZERO_DEMO} USCITA 2 FORMATO testo ERRORE K-ERR-020
            INGRESSO "aaaaaaaaaaaaaaaa"
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
kzero_prova(cassaforte_chiudi_senza_password BASE ${KZERO_DEMO} USCITA 2 FORMATO testo ERRORE K-ERR-019
            ARGOMENTI chiudi @PACCHETTO@ --scrivi @NUOVA@ --utente A --conservazione 2030-12-31 --password-da-ingresso)
# Pacchetto con cassaforte senza --utente; --utente su un pacchetto senza cassaforte; password sbagliata; utente sconosciuto.
kzero_prova(cassaforte_serve_utente BASE ${KZERO_CASSAFORTE} USCITA 2 FORMATO testo ERRORE K-ERR-032
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(cassaforte_utente_senza_cassaforte USCITA 2 FORMATO testo ERRORE K-ERR-031
            ARGOMENTI controlla ${KZERO_DEMO} --utente A)
kzero_prova(cassaforte_password_sbagliata BASE ${KZERO_CASSAFORTE} USCITA 2 FORMATO testo ERRORE K-ERR-022
            INGRESSO "${PW_UFFICIO}" ARGOMENTI controlla @PACCHETTO@ --utente TITOLARE --password-da-ingresso)
kzero_prova(cassaforte_password_sbagliata_json BASE ${KZERO_CASSAFORTE} USCITA 2 FORMATO json SCHEMA errore ERRORE K-ERR-022
            INGRESSO "${PW_UFFICIO}" ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --utente TITOLARE --password-da-ingresso --formato json)
kzero_prova(cassaforte_utente_sconosciuto BASE ${KZERO_CASSAFORTE} USCITA 2 FORMATO testo ERRORE K-ERR-021
            INGRESSO "${PW_TITOLARE}" ARGOMENTI controlla @PACCHETTO@ --utente NESSUNO --password-da-ingresso)

# Manomissioni: un byte cambiato nell'intestazione, nella fessura di un altro utente, nei dati; cassaforte troncata, vuota,
# di un altro pacchetto, di una versione sconosciuta, con parametri assurdi; registro cambiato o mancante.
foreach(caso "intestazione|100|K-ERR-028" "fessura_di_un_altro|-1|K-ERR-028" "versione|8|K-ERR-025" "parametri_assurdi|171|K-ERR-027")
  string(REPLACE "|" ";" parti "${caso}")
  list(GET parti 0 n)
  list(GET parti 1 posizione)
  list(GET parti 2 errore)
  if(n STREQUAL "fessura_di_un_altro")
    set(posizione 380)   # nella chiave pubblica di UFFICIO: TITOLARE apre la sua fessura, l'intestazione non torna
  endif()
  kzero_cassaforte(cassaforte_manomessa_${n} TITOLARE USCITA 1 FORMATO testo ERRORE ${errore}
                   NON_CONTIENE ${DATI_PERSONALI} MODIFICA BYTE cassaforte.kzc ${posizione}
                   ARGOMENTI controlla @PACCHETTO@)
endforeach()
kzero_cassaforte(cassaforte_manomessa_dati TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-028 MODIFICA BYTE cassaforte.kzc -1
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --includi-dati-personali)
kzero_cassaforte(cassaforte_troncata TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-024 MODIFICA TRONCA cassaforte.kzc 400
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_vuota TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-024 MODIFICA TRONCA cassaforte.kzc 0
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_di_un_altro_pacchetto TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-026
                 MODIFICA IMPOSTA pacchetto.json /cassaforte/id "\"0123456789abcdef0123456789abcdef\""
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_registro_cambiato TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-029 MODIFICA BYTE registro.kzr -3
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_registro_mancante TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-029 MODIFICA ELIMINA registro.kzr
                 ARGOMENTI controlla @PACCHETTO@)
kzero_prova(cassaforte_mancante BASE ${KZERO_CASSAFORTE} USCITA 1 FORMATO testo RESOCONTO MODIFICA ELIMINA cassaforte.kzc
            PROBLEMI "K-CTR-091|pacchetto.json|/cassaforte" CONTIENE "Controlli sul contenuto non eseguiti"
            ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_mancante_comando TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-038 MODIFICA ELIMINA cassaforte.kzc
                 ARGOMENTI cassaforte @PACCHETTO@)
kzero_prova(cassaforte_non_dichiarata BASE ${KZERO_CASSAFORTE} USCITA 1 FORMATO testo RESOCONTO
            MODIFICA IMPOSTA pacchetto.json /cassaforte false PROBLEMI "K-CTR-092|pacchetto.json|/cassaforte"
            ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_altra_chiusura TITOLARE USCITA 1 FORMATO testo RESOCONTO
                 MODIFICA IMPOSTA pacchetto.json /cassaforte/generazione 2
                 PROBLEMI "K-CTR-093|pacchetto.json|/cassaforte/generazione"
                 ARGOMENTI controlla @PACCHETTO@)
# Un elemento tolto nei file normali dopo la chiusura: i suoi valori non trovano posto (K-CTR-094), mai sull'elemento sbagliato.
kzero_cassaforte(cassaforte_elemento_tolto TITOLARE USCITA 1 FORMATO testo RESOCONTO MODIFICA TOGLI persone.json /persone/1
                 PROBLEMI "K-CTR-094|persone.json|/persone" NON_CONTIENE ${DATI_PERSONALI}
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_valore_doppio TITOLARE USCITA 1 FORMATO testo RESOCONTO
                 MODIFICA IMPOSTA persone.json /persone/0/nome "\"Nome in chiaro\""
                 PROBLEMI "K-CTR-095|persone.json|/persone/0" ARGOMENTI controlla @PACCHETTO@)

# Utenti e permessi. Avvisi sulle password (aggiunta 1 del K-010, testi nel catalogo dei controlli, uguali in testo e JSON):
# K-CTR-100 dopo aggiungi-utente e password --per-utente, K-CTR-099 dopo togli-utente, K-CTR-098 a ogni apertura di chi ha
# ricevuto chiavi nuove in una busta.
set(AVVISO_NUOVO "L'utente NUOVO e' stato aggiunto con una password scritta dall'utente TITOLARE: NUOVO la cambi al primo uso (kzero password), cosi' la conosce solo chi la usa.")
set(AVVISO_TOLTO "L'utente UFFICIO e' stato tolto e le chiavi sono state rifatte. Se conosceva la password di un altro utente (scritta, detta, uguale a un'altra), con quella potrebbe aprire anche la cassaforte nuova: l'utente TITOLARE cambi subito la sua (kzero password). Agli altri il programma lo ricorda a ogni apertura, finche' non la cambiano (K-CTR-098).")
set(AVVISO_BUSTA "Le chiavi della cassaforte sono state rifatte (un utente e' stato tolto) e l'utente SALUTE le ha ricevute in una busta, che si apre con la sua password: se l'utente tolto la conosceva, potrebbe aprire anche la cassaforte nuova. Cambiare la password (kzero password): finche' non si cambia, ogni apertura lo ricorda.")
kzero_cassaforte(cassaforte_aggiungi_utente TITOLARE USCITA 0 FORMATO testo INGRESSO_IN_PIU "una password nuova e lunga"
                 CONTIENE "Aggiunto l'utente NUOVO con i dati personali." "NUOVO (dati personali)" "Registro degli accessi: 4 voci."
                          "Avviso K-CTR-100: ${AVVISO_NUOVO}"
                 NON_CONTIENE "Avviso K-CTR-098"
                 ARGOMENTI aggiungi-utente --scrivi @PACCHETTO@ --nuovo-utente NUOVO --permessi personali)
kzero_cassaforte(cassaforte_aggiungi_utente_json TITOLARE USCITA 0 FORMATO json SCHEMA cassaforte
                 INGRESSO_IN_PIU "una password nuova e lunga"
                 CONTIENE "\"codice\": \"K-CTR-100\"" "\"messaggio\": \"${AVVISO_NUOVO}\""
                 ARGOMENTI aggiungi-utente --scrivi @PACCHETTO@ --nuovo-utente NUOVO --permessi personali --formato json)
kzero_cassaforte(cassaforte_aggiungi_utente_oltre_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 INGRESSO_IN_PIU "una password nuova e lunga"
                 ARGOMENTI aggiungi-utente --scrivi @PACCHETTO@ --nuovo-utente NUOVO --permessi tutti)
kzero_cassaforte(cassaforte_aggiungi_utente_esistente TITOLARE USCITA 2 FORMATO testo ERRORE K-ERR-036
                 INGRESSO_IN_PIU "una password nuova e lunga"
                 ARGOMENTI aggiungi-utente --scrivi @PACCHETTO@ --nuovo-utente UFFICIO --permessi personali)
kzero_cassaforte(cassaforte_togli_utente TITOLARE USCITA 0 FORMATO json SCHEMA cassaforte
                 CONTIENE "\"comando\": \"togli-utente\"" "\"codice\": \"K-CTR-099\"" "\"messaggio\": \"${AVVISO_TOLTO}\""
                 NON_CONTIENE "\"codice\": \"UFFICIO\"" "\"codice\": \"K-CTR-098\""
                 ARGOMENTI togli-utente --scrivi @PACCHETTO@ --utente-da-togliere UFFICIO --formato json)
kzero_cassaforte(cassaforte_togli_utente_testo TITOLARE USCITA 0 FORMATO testo
                 CONTIENE "Tolto l'utente UFFICIO" "Avviso K-CTR-099: ${AVVISO_TOLTO}" NON_CONTIENE "Avviso K-CTR-098"
                 ARGOMENTI togli-utente --scrivi @PACCHETTO@ --utente-da-togliere UFFICIO)
kzero_cassaforte(cassaforte_togli_utente_oltre_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI togli-utente --scrivi @PACCHETTO@ --utente-da-togliere SALUTE)
# Togliere un utente ricifra i dati legati a un luogo sensibile: servono tutti e due i permessi, anche per togliere chi ha i
# soli dati personali (aggiunta 1 del K-010).
kzero_cassaforte(cassaforte_togli_utente_servono_tutti_e_due_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 PREPARA aggiungi-utente --scrivi @PACCHETTO@ --utente UFFICIO --nuovo-utente NUOVO --permessi personali
                         --password-da-ingresso
                 PREPARA_INGRESSO "${PW_UFFICIO}" "una password nuova e lunga"
                 CONTIENE "Per togliere un utente servono i dati personali e particolari"
                 ARGOMENTI togli-utente --scrivi @PACCHETTO@ --utente-da-togliere NUOVO)
kzero_cassaforte(cassaforte_togli_ultimo_con_tutti_i_permessi TITOLARE USCITA 2 FORMATO testo ERRORE K-ERR-036
                 ARGOMENTI togli-utente --scrivi @PACCHETTO@ --utente-da-togliere TITOLARE)
# L'utente tolto non apre la versione nuova.
kzero_cassaforte(cassaforte_utente_tolto_non_apre UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-021
                 PREPARA togli-utente --scrivi @PACCHETTO@ --utente TITOLARE --utente-da-togliere UFFICIO --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}"
                 ARGOMENTI controlla @PACCHETTO@)
# Dopo il cambio delle chiavi chi resta apre (busta e storia delle chiavi).
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi SALUTE USCITA 0 FORMATO testo
                 PREPARA togli-utente --scrivi @PACCHETTO@ --utente TITOLARE --utente-da-togliere UFFICIO --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}"
                 CONTIENE "Cassaforte aperta dall'utente SALUTE" "Avviso K-CTR-098: ${AVVISO_BUSTA}" ARGOMENTI controlla @PACCHETTO@)
set(DOPO_IL_CAMBIO PREPARA togli-utente --scrivi @PACCHETTO@ --utente TITOLARE --utente-da-togliere UFFICIO --password-da-ingresso
                   PREPARA_INGRESSO "${PW_TITOLARE}")
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_json SALUTE USCITA 0 FORMATO json SCHEMA controlla ${DOPO_IL_CAMBIO}
                 CONTIENE "\"codice\": \"K-CTR-098\"" "\"messaggio\": \"${AVVISO_BUSTA}\""
                 ARGOMENTI controlla @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_giornata SALUTE USCITA 0 FORMATO testo ${DOPO_IL_CAMBIO}
                 CONTIENE "Cassaforte aperta dall'utente SALUTE (dati particolari).\nAvviso K-CTR-098: ${AVVISO_BUSTA}\n"
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_giornata_json SALUTE USCITA 0 FORMATO json SCHEMA giornata ${DOPO_IL_CAMBIO}
                 CONTIENE "\"codice\": \"K-CTR-098\"" "\"messaggio\": \"${AVVISO_BUSTA}\""
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --formato json)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_cassaforte SALUTE USCITA 0 FORMATO testo ${DOPO_IL_CAMBIO}
                 CONTIENE "Avviso K-CTR-098: ${AVVISO_BUSTA}" ARGOMENTI cassaforte @PACCHETTO@)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_cassaforte_json SALUTE USCITA 0 FORMATO json SCHEMA cassaforte
                 ${DOPO_IL_CAMBIO} CONTIENE "\"codice\": \"K-CTR-098\"" "\"messaggio\": \"${AVVISO_BUSTA}\""
                 ARGOMENTI cassaforte @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_registro SALUTE USCITA 0 FORMATO testo VARIABILI ${DOPO_IL_CAMBIO}
                 CONTIENE "Avviso K-CTR-098: ${AVVISO_BUSTA}" ARGOMENTI registro @PACCHETTO@)
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_registro_json SALUTE USCITA 0 FORMATO json SCHEMA registro VARIABILI
                 ${DOPO_IL_CAMBIO} CONTIENE "\"codice\": \"K-CTR-098\"" "\"messaggio\": \"${AVVISO_BUSTA}\""
                 ARGOMENTI registro @PACCHETTO@ --formato json)
# Chi ha tolto l'utente ha gia' le chiavi nuove nella sua fessura: nessun K-CTR-098.
kzero_cassaforte(cassaforte_dopo_il_cambio_delle_chiavi_chi_ha_tolto TITOLARE USCITA 0 FORMATO testo ${DOPO_IL_CAMBIO}
                 CONTIENE "Cassaforte aperta dall'utente TITOLARE" NON_CONTIENE "Avviso K-CTR-098" ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_password_cambiata TITOLARE USCITA 2 FORMATO testo ERRORE K-ERR-022
                 PREPARA password --scrivi @PACCHETTO@ --utente TITOLARE --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}" "la mia password nuova e lunga"
                 ARGOMENTI controlla @PACCHETTO@)
kzero_prova(cassaforte_password_nuova_apre BASE ${KZERO_CASSAFORTE} USCITA 0 FORMATO testo INGRESSO "la mia password nuova e lunga"
            PREPARA password --scrivi @PACCHETTO@ --utente TITOLARE --password-da-ingresso
            PREPARA_INGRESSO "${PW_TITOLARE}" "la mia password nuova e lunga"
            ARGOMENTI controlla @PACCHETTO@ --utente TITOLARE --password-da-ingresso)
kzero_cassaforte(cassaforte_password_per_un_altro_oltre_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 INGRESSO_IN_PIU "una password nuova e lunga"
                 ARGOMENTI password --scrivi @PACCHETTO@ --per-utente SALUTE)
kzero_prova(cassaforte_password_per_un_altro BASE ${KZERO_CASSAFORTE} USCITA 0 FORMATO testo INGRESSO "${PW_TITOLARE}" "una password nuova e lunga"
            CONTIENE "password nuova per l'utente SALUTE"
                     "Avviso K-CTR-100: La password nuova dell'utente SALUTE l'ha scritta l'utente TITOLARE: SALUTE la cambi al primo uso (kzero password), cosi' la conosce solo chi la usa."
            ARGOMENTI password --scrivi @PACCHETTO@ --utente TITOLARE --per-utente SALUTE --password-da-ingresso)
kzero_prova(cassaforte_password_per_se BASE ${KZERO_CASSAFORTE} USCITA 0 FORMATO testo
            INGRESSO "${PW_TITOLARE}" "la mia password nuova e lunga"
            CONTIENE "password cambiata dall'utente TITOLARE" NON_CONTIENE "Avviso K-CTR-100" "Avviso K-CTR-098"
            ARGOMENTI password --scrivi @PACCHETTO@ --utente TITOLARE --password-da-ingresso)

# Conservazione: passata, ogni apertura lo ricorda (orologio del PC).
kzero_cassaforte(cassaforte_conservazione_passata TITOLARE USCITA 0 FORMATO testo
                 PREPARA conservazione --scrivi @PACCHETTO@ --utente TITOLARE --conservazione 2020-01-01 --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}"
                 CONTIENE "Avviso K-CTR-097: La data di conservazione dei dati personali e particolari della cassaforte (2020-01-01) e' passata"
                          "(data passata)"
                 ARGOMENTI controlla @PACCHETTO@)
kzero_cassaforte(cassaforte_conservazione_passata_giornata UFFICIO USCITA 0 FORMATO testo
                 PREPARA conservazione --scrivi @PACCHETTO@ --utente TITOLARE --conservazione 2020-01-01 --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}"
                 CONTIENE "Avviso K-CTR-097: La data di conservazione dei dati personali e particolari della cassaforte (2020-01-01) e' passata"
                 ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --mezzo ALFA-03)
kzero_cassaforte(cassaforte_conservazione_senza_tutti_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI conservazione --scrivi @PACCHETTO@ --conservazione 2030-01-01)

# Uscita in chiaro e richiusura nella stessa cassaforte.
kzero_cassaforte(cassaforte_esporta TITOLARE USCITA 0 FORMATO testo
                 CONTIENE "Uscita in chiaro del pacchetto DEMO-2026" "correggerli, richiuderli (kzero richiudi"
                 ARGOMENTI esporta @PACCHETTO@ --scrivi @NUOVA@)
kzero_cassaforte(cassaforte_esporta_senza_tutti_i_permessi UFFICIO USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI esporta @PACCHETTO@ --scrivi @NUOVA@)
kzero_cassaforte(cassaforte_richiudi TITOLARE USCITA 0 FORMATO testo
                 PREPARA esporta @PACCHETTO@ --scrivi @NUOVA@ --utente TITOLARE --password-da-ingresso
                 PREPARA_INGRESSO "${PW_TITOLARE}"
                 CONTIENE "richiuso: dati sostituiti nella stessa cassaforte (chiusura numero 2)"
                          "Utenti: TITOLARE (dati personali e particolari), UFFICIO (dati personali), SALUTE (dati particolari)."
                 ARGOMENTI richiudi @NUOVA@ --scrivi @PACCHETTO@)
kzero_cassaforte(cassaforte_richiudi_altro_pacchetto TITOLARE USCITA 1 FORMATO testo ERRORE K-ERR-037
                 ARGOMENTI richiudi ${PROJECT_SOURCE_DIR}/esempi/demo-numeri --scrivi @PACCHETTO@)

# Cancellazione: prima le chiavi, poi i file; resoconto da conservare.
kzero_cassaforte(cassaforte_cancella TITOLARE USCITA 0 FORMATO testo VARIABILI
                 CONTIENE "cancellata il ORA (UTC) dall'utente TITOLARE" "Conteneva 248 valori personali e 37 particolari, di 3 utenti."
                          "restano, cifrate con le password di allora"
                 ARGOMENTI cancella --scrivi @PACCHETTO@)
kzero_cassaforte(cassaforte_cancella_json TITOLARE USCITA 0 FORMATO json SCHEMA cassaforte VARIABILI
                 CONTIENE "\"cassaforte\": null" "\"voci_registro\": 3"
                 ARGOMENTI cancella --scrivi @PACCHETTO@ --formato json)
kzero_cassaforte(cassaforte_cancella_senza_tutti_i_permessi SALUTE USCITA 2 FORMATO testo ERRORE K-ERR-023
                 ARGOMENTI cancella --scrivi @PACCHETTO@)
kzero_prova(cassaforte_dopo_la_cancellazione BASE ${KZERO_CASSAFORTE} USCITA 1 FORMATO testo RESOCONTO
            PREPARA cancella --scrivi @PACCHETTO@ --utente TITOLARE --password-da-ingresso PREPARA_INGRESSO "${PW_TITOLARE}"
            PROBLEMI "K-CTR-091|pacchetto.json|/cassaforte" ARGOMENTI controlla @PACCHETTO@)
# Dati personali in chiaro senza "dati_inventati": avviso a ogni lettura.
kzero_prova(dati_in_chiaro_senza_dati_inventati BASE ${KZERO_DEMO} USCITA 0 FORMATO testo
            MODIFICA TOGLI pacchetto.json /dati_inventati
            CONTIENE "Avviso K-CTR-096: Il pacchetto contiene 257 valori personali e 37 particolari in chiaro"
            ARGOMENTI controlla @PACCHETTO@)

# --- Documentazione e architettura --------------------------------------------------
add_test(NAME documentazione.errori_e_schemi
         COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/documentazione.cmake)
set_tests_properties(documentazione.errori_e_schemi PROPERTIES LABELS documentazione)
add_test(NAME architettura.nucleo_senza_stampa
         COMMAND ${CMAKE_COMMAND} -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/architettura.cmake)
set_tests_properties(architettura.nucleo_senza_stampa PROPERTIES LABELS architettura)

# --- Sicurezza (K-009): niente rete -----------------------------------------------------------------------------
# Il programma compilato non importa funzioni di rete (Linux: simboli ELF) e nessun sorgente include intestazioni o
# librerie di rete. Vedi docs/sicurezza.md.
add_test(NAME sicurezza.niente_rete_nei_sorgenti
         COMMAND ${CMAKE_COMMAND} -DMODO=sorgenti -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/rete.cmake)
set_tests_properties(sicurezza.niente_rete_nei_sorgenti PROPERTIES LABELS sicurezza)
# Sul programma vero: con i sanitizer il programma contiene anche le loro librerie, che intercettano le funzioni di
# rete del sistema (e si collegano a libresolv) senza che K-Zero le usi.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_NM AND CMAKE_OBJDUMP AND NOT KZERO_SANITIZER AND NOT KZERO_FUZZ)
  add_test(NAME sicurezza.niente_rete_nel_programma
           COMMAND ${CMAKE_COMMAND} -DMODO=programma -DKZERO=$<TARGET_FILE:kzero> -DNM=${CMAKE_NM} -DOBJDUMP=${CMAKE_OBJDUMP}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/rete.cmake)
  set_tests_properties(sicurezza.niente_rete_nel_programma PROPERTIES LABELS sicurezza)
endif()

# Protezioni del compilatore nella versione di rilascio per Linux (DECISIONI.md).
if(KZERO_PROTEZIONI_ATTIVE AND CMAKE_READELF AND CMAKE_NM)
  add_test(NAME sicurezza.protezioni_del_compilatore
           COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DREADELF=${CMAKE_READELF} -DNM=${CMAKE_NM}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/protezioni.cmake)
  set_tests_properties(sicurezza.protezioni_del_compilatore PROPERTIES LABELS sicurezza)
endif()

# Elenco dei componenti esterni (docs/componenti.cdx.json, CycloneDX): coincide con le versioni e le impronte fissate in
# CMakeLists.txt e in tests/strumenti/check-jsonschema.txt.
add_test(NAME sicurezza.elenco_dei_componenti
         COMMAND ${CMAKE_COMMAND} -DSORGENTI=${PROJECT_SOURCE_DIR} -DVERSIONE=${PROJECT_VERSION}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/componenti.cmake)
set_tests_properties(sicurezza.elenco_dei_componenti PROPERTIES LABELS sicurezza)

# --- Fuzzing (K-009): prove con dati casuali e deformati (tests/fuzz, docs/sicurezza.md) -------------------------
# I bersagli stanno in una libreria comune: libFuzzer li usa per cercare guasti (KZERO_FUZZ=ON, clang), rigioca_fuzz
# per ripetere nelle prove normali ogni guasto gia' trovato (tests/fuzz/casi/<bersaglio>/<caso>) e i file del demo.
add_library(kzero_bersagli_fuzz STATIC fuzz/bersagli.cpp)
target_link_libraries(kzero_bersagli_fuzz PUBLIC kzero_api)
target_compile_options(kzero_bersagli_fuzz PRIVATE ${KZERO_WARNINGS})
add_executable(rigioca_fuzz fuzz/rigioca_fuzz.cpp)
target_link_libraries(rigioca_fuzz PRIVATE kzero_bersagli_fuzz)
target_compile_options(rigioca_fuzz PRIVATE ${KZERO_WARNINGS})
add_test(NAME fuzz.semi COMMAND rigioca_fuzz --semi)
set_tests_properties(fuzz.semi PROPERTIES LABELS fuzz)
file(GLOB casi_fuzz LIST_DIRECTORIES true CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/casi/*)
foreach(cartella IN LISTS casi_fuzz)
  if(IS_DIRECTORY ${cartella})
    get_filename_component(bersaglio ${cartella} NAME)
    add_test(NAME fuzz.casi.${bersaglio} COMMAND rigioca_fuzz ${bersaglio} ${cartella})
    set_tests_properties(fuzz.casi.${bersaglio} PROPERTIES LABELS fuzz)
  endif()
endforeach()
if(KZERO_FUZZ)
  foreach(bersaglio pacchetto parametri date_e_orari cassaforte)
    add_executable(fuzz_${bersaglio} fuzz/fuzz_${bersaglio}.cpp)
    target_link_libraries(fuzz_${bersaglio} PRIVATE kzero_bersagli_fuzz)
    target_compile_options(fuzz_${bersaglio} PRIVATE ${KZERO_WARNINGS})
    target_link_options(fuzz_${bersaglio} PRIVATE -fsanitize=fuzzer)
  endforeach()
endif()

# --- Prova di scala: un pacchetto grande, generato con un seme fisso, controllato senza errori ------------
# Il tempo e la memoria si misurano a parte (DIARIO.md): le macchine del controllo GitHub variano.
add_executable(genera_scala genera_scala.cpp)
target_link_libraries(genera_scala PRIVATE kzero_api)
target_compile_options(genera_scala PRIVATE ${KZERO_WARNINGS})
add_test(NAME scala.genera COMMAND genera_scala ${CMAKE_CURRENT_BINARY_DIR}/scala)
set_tests_properties(scala.genera PROPERTIES FIXTURES_SETUP scala LABELS scala)
# Il riepilogo in due pezzi: in CMake il punto e virgola separa gli elementi di un elenco.
# Dal K-011 con le leggi sugli orari della dimostrazione: i turni le rispettano anche nella lettura prudente; resta solo la
# media su 4 mesi del D.Lgs. 234, che esce dal periodo del pacchetto (verifica parziale, mai "regolare").
kzero_prova(scala_controlla USCITA 0 FORMATO testo
            CONTIENE "10000 corse, 600 mezzi, 1320 persone, 1200 turni" " 0 errori, 1 avviso."
            PROBLEMI "K-CTR-104|norme.json|/norme/0/regimi/2/regole/2"
            ARGOMENTI controlla ${CMAKE_CURRENT_BINARY_DIR}/scala)
set_tests_properties(programma.scala_controlla PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")
# K-011: gli orari di tutte le 1.320 persone in un mese, e quelli di una persona in una settimana (un anno intero si misura
# a mano, DIARIO.md).
kzero_prova(scala_ore USCITA 0 FORMATO testo CONTIENE "Persona P-10-132" "Totali del periodo"
            ARGOMENTI ore ${CMAKE_CURRENT_BINARY_DIR}/scala --dal 2026-10-01 --al 2026-10-31)
set_tests_properties(programma.scala_ore PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")
kzero_prova(scala_ore_persona USCITA 0 FORMATO json SCHEMA ore CONTIENE "\"codice\": \"P-03-007\""
            ARGOMENTI ore ${CMAKE_CURRENT_BINARY_DIR}/scala --dal 2026-10-12 --al 2026-10-18 --persona P-03-007 --formato json)
set_tests_properties(programma.scala_ore_persona PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")
# K-008: la giornata di un giovedi' di scuola (tutti i turni) e quella di una persona finiscono senza errori.
kzero_prova(scala_giornata USCITA 0 FORMATO testo CONTIENE "Totali: 1200 turni (nessuno scoperto, nessuno senza mezzo)"
            ARGOMENTI giornata ${CMAKE_CURRENT_BINARY_DIR}/scala --data 2026-10-15)
set_tests_properties(programma.scala_giornata PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")
kzero_prova(scala_giornata_persona USCITA 0 FORMATO json SCHEMA giornata CONTIENE "\"turni\": 1,"
            ARGOMENTI giornata ${CMAKE_CURRENT_BINARY_DIR}/scala --data 2026-10-15 --persona P-03-007 --formato json)
set_tests_properties(programma.scala_giornata_persona PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")

# --- Schemi: validi per JSON Schema draft-07; esempi e pacchetti di esempio conformi -----
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
  # I file dei pacchetti di esempio, con lo strumento standard (indipendente dalla libreria del programma); anche
  # la copia congelata del demo del K-007, scritta con le versioni precedenti degli schemi.
  foreach(pacchetto demo demo-numeri tests/esempi/k007-demo tests/esempi/k007-demo-numeri)
    if(pacchetto MATCHES "^tests/")
      set(cartella ${PROJECT_SOURCE_DIR}/${pacchetto})
      get_filename_component(pacchetto ${pacchetto} NAME)
    else()
      set(cartella ${PROJECT_SOURCE_DIR}/esempi/${pacchetto})
    endif()
    file(GLOB file_dati CONFIGURE_DEPENDS ${cartella}/*.json)
    foreach(f IN LISTS file_dati)
      get_filename_component(nome ${f} NAME_WE)
      add_test(NAME schema.${pacchetto}.${nome}
               COMMAND ${CMAKE_COMMAND} -DVALIDATORE=${KZERO_CHECK_JSONSCHEMA} -DSORGENTI=${PROJECT_SOURCE_DIR}
                       -DDOCUMENTO=${f} -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/schema.cmake)
      set_tests_properties(schema.${pacchetto}.${nome} PROPERTIES LABELS schemi)
    endforeach()
  endforeach()
endif()
