# K-Zero - prepara le prove con la cassaforte vera su un pacchetto (K-011 aggiunta 2: quello degli orari; K-012: il demo): lo chiude in
# una cartella nuova con tre utenti, TIT (dati personali e particolari), UFF (solo personali) e SAL (solo particolari).
# Uso: cmake -DKZERO=<kzero> -DORIGINE=<cartella in chiaro> -DDESTINAZIONE=<cartella nuova>
#            -DPW_TIT=<password> -DPW_UFF=<password> -DPW_SAL=<password> -P chiudi_k011.cmake
# Le password vanno sull'ingresso standard (--password-da-ingresso), mai come parametri.
foreach(v KZERO ORIGINE DESTINAZIONE PW_TIT PW_UFF PW_SAL)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "manca -D${v}")
  endif()
endforeach()
file(REMOVE_RECURSE "${DESTINAZIONE}")
set(ingresso "${DESTINAZIONE}.ingresso")

function(lancia testo)
  file(WRITE "${ingresso}" "${testo}")
  execute_process(COMMAND "${KZERO}" ${ARGN} INPUT_FILE "${ingresso}" RESULT_VARIABLE esito OUTPUT_VARIABLE uscita
                  ERROR_VARIABLE errori)
  file(REMOVE "${ingresso}")
  if(NOT esito EQUAL 0)
    message(FATAL_ERROR "kzero ${ARGN}: uscita ${esito}\n${uscita}\n${errori}")
  endif()
endfunction()

lancia("${PW_TIT}\n" chiudi "${ORIGINE}" --scrivi "${DESTINAZIONE}" --utente TIT --conservazione 2030-12-31 --password-da-ingresso)
lancia("${PW_TIT}\n${PW_UFF}\n" aggiungi-utente --scrivi "${DESTINAZIONE}" --utente TIT --nuovo-utente UFF --permessi personali
       --password-da-ingresso)
lancia("${PW_TIT}\n${PW_SAL}\n" aggiungi-utente --scrivi "${DESTINAZIONE}" --utente TIT --nuovo-utente SAL --permessi particolari
       --password-da-ingresso)
