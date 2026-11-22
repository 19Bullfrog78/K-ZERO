# K-Zero - prepara le prove sul programma del tipo di servizio per scheda (K-024): il pacchetto di prova (demo, codici esterni e profili
# di tests/esempi/k019-elenco, i codici esterni di tests/esempi/k021-schede e il profilo alla versione 3 di tests/esempi/k024-schede, con la
# regola del tipo e la colonna dei disabili) chiuso con kzero chiudi dall'utente TITOLARE (tutti e due i permessi), con l'utente PERSONALI
# (solo i dati personali); il file Excel delle quattro schede di prova (una scolastica, una per disabili dalla regola, una promiscua con 1
# disabile, una per disabili con 0) costruito dai testi dei quattro fogli (nessun .xlsx nel deposito). Le prove lavorano sempre su una
# copia del pacchetto chiuso (BASE).
# Uso: cmake -DKZERO=<kzero> -DPREPARA=<prepara_k019> -DCREA=<crea_xlsx> -DELENCO=<tests/esempi/k019-elenco>
#            -DSCHEDE21=<tests/esempi/k021-schede> -DSORGENTI=<tests/esempi/k024-schede> -DUSCITA=<cartella> -P prepara_k024.cmake
cmake_minimum_required(VERSION 3.25)
file(REMOVE_RECURSE "${USCITA}")
file(MAKE_DIRECTORY "${USCITA}")
execute_process(COMMAND "${PREPARA}" "${ELENCO}" "${USCITA}/chiaro" "${SCHEDE21}" "${SORGENTI}" RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "prepara_k019: ${r}")
endif()
file(WRITE "${USCITA}/pw" "una frase lunga per le schede di prova\n")
execute_process(COMMAND "${KZERO}" chiudi "${USCITA}/chiaro" --scrivi "${USCITA}/chiuso" --utente TITOLARE --conservazione 2099-12-31
                        --password-da-ingresso
                INPUT_FILE "${USCITA}/pw" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero chiudi: ${r}")
endif()
file(WRITE "${USCITA}/pw2" "una frase lunga per le schede di prova\nun'altra frase lunga per chi vede i dati personali\n")
execute_process(COMMAND "${KZERO}" aggiungi-utente --scrivi "${USCITA}/chiuso" --utente TITOLARE --nuovo-utente PERSONALI --permessi personali
                        --password-da-ingresso
                INPUT_FILE "${USCITA}/pw2" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero aggiungi-utente: ${r}")
endif()
file(REMOVE_RECURSE "${USCITA}/chiaro")
execute_process(COMMAND "${CREA}" --fogli "${USCITA}/k024.xlsx" "Schede=${SORGENTI}/schede.tsv" "Fermate=${SORGENTI}/fermate.tsv"
                        "Date=${SORGENTI}/date.tsv" "Interruzioni=${SORGENTI}/interruzioni.tsv"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx: ${r}")
endif()
