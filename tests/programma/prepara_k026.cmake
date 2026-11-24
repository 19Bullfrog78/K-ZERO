# K-Zero - prepara le prove sul programma del mezzo della scheda (K-026): il pacchetto di prova (demo, codici esterni e profili di
# tests/esempi/k019-elenco, i codici esterni di k021-schede, k023-schede e k024-schede, i due profili alla versione 4 di tests/esempi/k026-schede:
# PS-CVA con la colonna "Mezzo", PS-CVA-SM senza) chiuso con kzero chiudi dall'utente TITOLARE (tutti e due i permessi); i file Excel delle tre
# schede di prova (k026.xlsx: la targa con gli spazi e il trattino, una targa che non c'e', la cella vuota) e lo stesso file con il mezzo della
# 4000801/1 cambiato (k026-cambiato.xlsx), costruiti dai testi dei fogli (nessun .xlsx nel deposito). Le prove lavorano sempre su una copia del
# pacchetto chiuso (BASE). Dati inventati.
# Uso: cmake -DKZERO=<kzero> -DPREPARA=<prepara_k019> -DCREA=<crea_xlsx> -DELENCO=<tests/esempi/k019-elenco> -DESEMPI=<tests/esempi>
#            -DUSCITA=<cartella> -P prepara_k026.cmake
cmake_minimum_required(VERSION 3.25)
file(REMOVE_RECURSE "${USCITA}")
file(MAKE_DIRECTORY "${USCITA}")
execute_process(COMMAND "${PREPARA}" "${ELENCO}" "${USCITA}/chiaro" "${ESEMPI}/k021-schede" "${ESEMPI}/k023-schede" "${ESEMPI}/k024-schede"
                        "${ESEMPI}/k026-schede"
                RESULT_VARIABLE r)
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
set(S "${ESEMPI}/k026-schede")
execute_process(COMMAND "${CREA}" --fogli "${USCITA}/k026.xlsx" "Schede=${S}/schede.tsv" "Fermate=${S}/fermate.tsv" "Date=${S}/date.tsv"
                        "Interruzioni=${S}/interruzioni.tsv"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx: ${r}")
endif()
file(READ "${S}/schede.tsv" schede)
string(REPLACE "AA 101-AA" "AA 104 AA" schede "${schede}")
file(WRITE "${USCITA}/schede-cambiate.tsv" "${schede}")
execute_process(COMMAND "${CREA}" --fogli "${USCITA}/k026-cambiato.xlsx" "Schede=${USCITA}/schede-cambiate.tsv" "Fermate=${S}/fermate.tsv"
                        "Date=${S}/date.tsv" "Interruzioni=${S}/interruzioni.tsv"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx (mezzo cambiato): ${r}")
endif()
