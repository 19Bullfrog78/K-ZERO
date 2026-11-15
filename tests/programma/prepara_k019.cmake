# K-Zero - prepara le prove sul programma dell'elenco utenti (K-019): il pacchetto di prova (demo, profili e codici esterni di
# tests/esempi/k019-elenco) chiuso con kzero chiudi, e i file Excel costruiti dai testi dei file di prova. Le prove lavorano
# sempre su una copia del pacchetto chiuso (BASE).
# Uso: cmake -DKZERO=<kzero> -DPREPARA=<prepara_k019> -DCREA=<crea_xlsx> -DSORGENTI=<tests/esempi/k019-elenco> -DUSCITA=<cartella>
#            -P prepara_k019.cmake
cmake_minimum_required(VERSION 3.25)
file(REMOVE_RECURSE "${USCITA}")
file(MAKE_DIRECTORY "${USCITA}")
execute_process(COMMAND "${PREPARA}" "${SORGENTI}" "${USCITA}/chiaro" RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "prepara_k019: ${r}")
endif()
file(WRITE "${USCITA}/pw" "una frase lunga per l'elenco di prova\n")
execute_process(COMMAND "${KZERO}" chiudi "${USCITA}/chiaro" --scrivi "${USCITA}/chiuso" --utente TITOLARE --conservazione 2099-12-31
                        --password-da-ingresso
                INPUT_FILE "${USCITA}/pw" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero chiudi: ${r}")
endif()
file(REMOVE_RECURSE "${USCITA}/chiaro")
foreach(nome prima-lettura seconda-lettura)
  execute_process(COMMAND "${CREA}" "${SORGENTI}/${nome}.tsv" "${USCITA}/${nome}.xlsx" RESULT_VARIABLE r)
  if(NOT r EQUAL 0)
    message(FATAL_ERROR "crea_xlsx ${nome}: ${r}")
  endif()
endforeach()
