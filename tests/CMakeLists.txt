# Prove automatiche: ctest --test-dir build --output-on-failure
# Etichette: nucleo, api, programma, documentazione, architettura, schemi, scala.
include(GoogleTest)

# --- Prove del nucleo (GoogleTest): ogni TEST si registra da solo --------------------
# I dati delle prove vengono dal pacchetto demo (esempi/demo), letto con l'API come lo legge il programma:
# per questo le prove del nucleo che usano i dati sono collegate anche a kzero_api.
add_executable(test_tempo test_tempo.cpp)
target_link_libraries(test_tempo PRIVATE kzero_nucleo GTest::gtest_main)
target_compile_options(test_tempo PRIVATE ${KZERO_WARNINGS})
gtest_discover_tests(test_tempo PROPERTIES LABELS nucleo)
foreach(prova test_regole test_dimostrazione test_ricerca test_pacchetto test_controlli test_turni)
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
#             [RESOCONTO] [PROBLEMI <K-CTR-nnn|file|percorso JSON>...]
#             [BASE <cartella> MODIFICA <operazione> <argomenti>...]
#             ARGOMENTI <argomenti>)
# RESOCONTO: il risultato va sull'uscita standard anche con l'uscita 1 (kzero controlla su dati sbagliati).
# BASE e MODIFICA: il pacchetto da controllare e' una copia di BASE con una sola modifica (vedi
# tests/programma/pacchetto.cmake); negli ARGOMENTI la sua cartella si scrive @PACCHETTO@.
# Per ogni prova in json registra anche schema.<nome>: il documento rispetta lo schema che dichiara.
set(KZERO_USCITE_PROVE ${CMAKE_CURRENT_BINARY_DIR}/uscite)

function(kzero_prova nome)
  cmake_parse_arguments(P "RESOCONTO" "USCITA;FORMATO;ERRORE;SCHEMA;ATTESO;USCITA_SU;BASE"
                        "ARGOMENTI;CONTIENE;NON_CONTIENE;PROBLEMI;MODIFICA" ${ARGN})
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
  if(DEFINED P_BASE)
    string(APPEND testo "set(PACCHETTO_BASE [==[${P_BASE}]==])\n")
  endif()
  foreach(elenco ARGOMENTI CONTIENE NON_CONTIENE PROBLEMI MODIFICA)
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
set(DATI_PERSONALI "Bianchi Luca" "Esposito Sara" "Gallo Pietro" "Casa Bianchi" "Casa Neri"
                   "abitazione di un autista" "Spostata da Lagobello")
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
kzero_prova(dimostrazione_testo_con_interruttore USCITA 0 FORMATO testo
            ATTESO ${CMAKE_CURRENT_SOURCE_DIR}/attesi/dimostrazione_dati_personali.txt
            CONTIENE ${DATI_PERSONALI} ${FERMATE_PUBBLICHE} NON_CONTIENE "Deambulatore"
            ARGOMENTI dimostrazione --includi-dati-personali)
kzero_prova(dimostrazione_testo_dati_particolari USCITA 0 FORMATO testo
            CONTIENE "Deambulatore" "tipo: disabili" "categoria: ibrido" ${FERMATE_PUBBLICHE} NON_CONTIENE ${DATI_PERSONALI}
            ARGOMENTI dimostrazione --includi-dati-particolari)
kzero_prova(dimostrazione_json USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": false" "\"dati_particolari_inclusi\": false" ${FERMATE_PUBBLICHE}
            NON_CONTIENE ${DATI_PERSONALI} ${DATI_PARTICOLARI}
            ARGOMENTI dimostrazione --formato json)
kzero_prova(dimostrazione_json_dati_personali USCITA 0 FORMATO json SCHEMA dimostrazione
            CONTIENE "\"dati_personali_inclusi\": true" ${DATI_PERSONALI} ${FERMATE_PUBBLICHE}
            NON_CONTIENE ${DATI_PARTICOLARI}
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
                       " 0 errori, 8 avvisi."
              PROBLEMI ${AVVISI_DEMO} NON_CONTIENE "Errore K-CTR" "abilitazioni" "assenze.json" "Problemi sui dati particolari"
              ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/${pacchetto})
  kzero_prova(controlla_${n}_json USCITA 0 FORMATO json SCHEMA controlla
              CONTIENE "\"errori\": 0" "\"avvisi\": 8" "\"posizione_completa_con\": \"--includi-dati-personali\""
              PROBLEMI ${AVVISI_DEMO} NON_CONTIENE "abilitazioni" "assenze.json"
              ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/${pacchetto} --formato json)
endforeach()
kzero_prova(controlla_demo_dati_personali USCITA 0 FORMATO testo PROBLEMI ${AVVISI_DEMO_COMPLETI} NON_CONTIENE "il punto preciso"
            ARGOMENTI controlla ${PROJECT_SOURCE_DIR}/esempi/demo --includi-dati-personali)
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
            PROBLEMI "K-CTR-012|persone.json|/persone/0" NON_CONTIENE "2026-10-21" "2026-10-22" "2026-10-23" "assenze")
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
              PROBLEMI "K-CTR-008|archivio" CONTIENE " 0 errori, 9 avvisi."
              ARGOMENTI controlla @PACCHETTO@)
  kzero_prova(giornata_con_una_pipe USCITA 1 FORMATO testo ERRORE K-ERR-018 BASE ${KZERO_DEMO} MODIFICA PIPE turni.json
              ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22)
  foreach(prova rotto_pipe_al_posto_del_file rotto_collegamento_a_dev_zero rotto_collegamento_fuori_dalla_cartella
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
# Principio del punto 12: un fatto con le date su un turno ricorrente e' un avviso in kzero controlla, e in kzero
# giornata quel giorno il turno e' scoperto o senza mezzo, con il motivo. Lo stesso fatto su un'assegnazione del
# singolo giorno e' un errore: kzero giornata non mostra nulla.
kzero_prova(avviso_assenza_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/3
            PROBLEMI "K-CTR-078|persone.json|/persone/0" CONTIENE " 0 errori, 9 avvisi." NON_CONTIENE "assenze.json"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(giornata_assenza_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/3
            CONTIENE "Persona: nessuna - TURNO SCOPERTO: la persona prevista P-A non puo' farlo (dati personali: riservati)."
                     "Prevista: P-A, rotazione ROT-ALFA, passo 11"
            NON_CONTIENE "FERIE"
            ARGOMENTI giornata @PACCHETTO@ --data 2026-10-22 --persona P-A)
kzero_prova(avviso_fermo_senza_sostituzione USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI assegnazioni.json /assegnazioni/0
            PROBLEMI "K-CTR-079|fermi.json|/fermi/0" CONTIENE " 0 errori, 9 avvisi."
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
                     "1 errore, 8 avvisi."
            NON_CONTIENE "Moretti Luca" "serve una di")
kzero_rotto(rotto_autista_senza_abilitazione_json FORMATO json MODIFICA TOGLI persone.json /persone/10/abilitazioni/1
            PROBLEMI "K-CTR-068|servizi_fissi.json|/servizi_fissi/4")
# Senza la distrazione il mezzo di linea ALFA-05 non puo' fare la gita, che chiede l'uso noleggio. Sparisce anche
# l'avviso sulla distrazione che scade.
kzero_rotto(rotto_mezzo_senza_uso_richiesto FORMATO testo MODIFICA TOGLI mezzi.json /mezzi/6/autorizzazioni_uso/0
            PROBLEMI "K-CTR-069|turni.json|/turni/11/mezzo"
            CONTIENE "Nel turno T-GITA (Gita) il mezzo ALFA-05 (AA 105 AA) (uso LINEA) fa il servizio S-GITA (Gita della Pro Loco al lago di Lagobello), che chiede l'uso NCC, e non ha un'autorizzazione a quell'uso (distrazione), il 2026-11-15."
                     "1 errore, 7 avvisi.")
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
            PROBLEMI "K-CTR-037|servizi.json|/servizi/2/domanda/gruppi/0" CONTIENE "0 errori, 9 avvisi."
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
kzero_prova(avviso_azienda_senza_norme USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA TOGLI aziende.json /aziende/0/norme PROBLEMI "K-CTR-052|pacchetto.json|/aziende/0"
            ARGOMENTI controlla @PACCHETTO@)
kzero_prova(avviso_abilitazione_che_scade USCITA 0 FORMATO testo BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA persone.json /persone/2/abilitazioni/2/scadenza "\"2027-03-31\""
            PROBLEMI "K-CTR-050|persone.json|/persone/2"
            CONTIENE "Un'abilitazione o un'autorizzazione della persona P-C scade dentro il periodo del pacchetto"
            NON_CONTIENE "CQC" "2027-03-31" "Verdi Carlo"
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
kzero_prova(classi_trasbordo_in_una_casa_dati_personali USCITA 1 FORMATO json SCHEMA controlla RESOCONTO BASE ${KZERO_DEMO}
            MODIFICA IMPOSTA corse.json /corse/4/passaggi/2/luogo "\"PP-01\""
            PROBLEMI "K-CTR-023|corse.json|/corse/4/passaggi/2/trasbordi/0" CONTIENE "PP-01 (Casa Bianchi)"
            ARGOMENTI controlla @PACCHETTO@ --includi-dati-personali --formato json)
# Dal K-009 un problema che rivela un dato particolare (carrozzine, assistente, tipo "disabili") di base non si elenca:
# una riga sola, senza codice ne' posizione, con quanti errori e avvisi sono; il codice di uscita non cambia.
kzero_rotto(classi_carrozzine FORMATO testo MODIFICA IMPOSTA servizi.json /servizi/3/domanda/gruppi/0/fermate/0/carrozzine 0
            CONTIENE "Problemi sui dati particolari (salute), non elencati uno per uno: 1 errore e nessun avviso." " 1 errore, 8 avvisi."
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

# --- Documentazione e architettura --------------------------------------------------
add_test(NAME documentazione.errori_e_schemi
         COMMAND ${CMAKE_COMMAND} -DKZERO=$<TARGET_FILE:kzero> -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/documentazione.cmake)
set_tests_properties(documentazione.errori_e_schemi PROPERTIES LABELS documentazione)
add_test(NAME architettura.nucleo_senza_stampa
         COMMAND ${CMAKE_COMMAND} -DSORGENTI=${PROJECT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/programma/architettura.cmake)
set_tests_properties(architettura.nucleo_senza_stampa PROPERTIES LABELS architettura)

# --- Prova di scala: un pacchetto grande, generato con un seme fisso, controllato senza errori ------------
# Il tempo e la memoria si misurano a parte (DIARIO.md): le macchine del controllo GitHub variano.
add_executable(genera_scala genera_scala.cpp)
target_link_libraries(genera_scala PRIVATE kzero_api)
target_compile_options(genera_scala PRIVATE ${KZERO_WARNINGS})
add_test(NAME scala.genera COMMAND genera_scala ${CMAKE_CURRENT_BINARY_DIR}/scala)
set_tests_properties(scala.genera PROPERTIES FIXTURES_SETUP scala LABELS scala)
# Il riepilogo in due pezzi: in CMake il punto e virgola separa gli elementi di un elenco.
kzero_prova(scala_controlla USCITA 0 FORMATO testo
            CONTIENE "10000 corse, 600 mezzi, 1320 persone, 1200 turni" " 0 errori, 0 avvisi."
            ARGOMENTI controlla ${CMAKE_CURRENT_BINARY_DIR}/scala)
set_tests_properties(programma.scala_controlla PROPERTIES FIXTURES_REQUIRED scala LABELS "programma;scala")
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
