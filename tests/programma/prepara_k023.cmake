# K-Zero - prepara le prove sul programma delle interruzioni e delle coincidenze delle schede (K-023): il pacchetto di prova (demo,
# codici esterni e profili di tests/esempi/k019-elenco e tests/esempi/k023-schede, profili alla versione 2) chiuso con kzero chiudi, e il
# file Excel delle tre schede per disabili (una con tre interruzioni, due in coincidenza) costruito dai testi dei quattro fogli (nessun
# .xlsx nel deposito). Le prove lavorano sempre su una copia del pacchetto chiuso (BASE).
# Uso: cmake -DKZERO=<kzero> -DPREPARA=<prepara_k019> -DCREA=<crea_xlsx> -DELENCO=<tests/esempi/k019-elenco>
#            -DSORGENTI=<tests/esempi/k023-schede> -DUSCITA=<cartella> -P prepara_k023.cmake
cmake_minimum_required(VERSION 3.25)
file(REMOVE_RECURSE "${USCITA}")
file(MAKE_DIRECTORY "${USCITA}")
execute_process(COMMAND "${PREPARA}" "${ELENCO}" "${USCITA}/chiaro" "${SORGENTI}" RESULT_VARIABLE r)
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
file(REMOVE_RECURSE "${USCITA}/chiaro")
execute_process(COMMAND "${CREA}" --fogli "${USCITA}/k023.xlsx" "Schede=${SORGENTI}/schede.tsv" "Fermate=${SORGENTI}/fermate.tsv"
                        "Date=${SORGENTI}/date.tsv" "Interruzioni=${SORGENTI}/interruzioni.tsv"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx: ${r}")
endif()
