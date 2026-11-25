# K-Zero - prepara le prove sul programma dei punti degli utenti (K-027): il pacchetto di prova (demo, profili e codici esterni di
# tests/esempi/k019-elenco, i codici esterni di k021-schede, k023-schede e k024-schede, i profili delle schede alla versione 4 di k026-schede e
# lo stradario di tests/esempi/k027-punti) chiuso con kzero chiudi dall'utente TITOLARE (tutti e due i permessi); i file Excel delle quattro
# schede (k027-schede.xlsx) e degli otto alunni (k027-elenco.xlsx), costruiti dai testi (nessun .xlsx nel deposito). Poi, sulla copia
# "letto", le schede e l'elenco letti con il programma (kzero leggi-schede e kzero leggi-elenco): le prove partono da li' (BASE). Dati inventati.
# Uso: cmake -DKZERO=<kzero> -DPREPARA=<prepara_k019> -DCREA=<crea_xlsx> -DELENCO=<tests/esempi/k019-elenco> -DESEMPI=<tests/esempi>
#            -DUSCITA=<cartella> -P prepara_k027.cmake
cmake_minimum_required(VERSION 3.25)
file(REMOVE_RECURSE "${USCITA}")
file(MAKE_DIRECTORY "${USCITA}")
execute_process(COMMAND "${PREPARA}" "${ELENCO}" "${USCITA}/chiaro" "${ESEMPI}/k021-schede" "${ESEMPI}/k023-schede" "${ESEMPI}/k024-schede"
                        "${ESEMPI}/k026-schede"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "prepara_k019: ${r}")
endif()
file(COPY "${ESEMPI}/k027-punti/stradari.json" DESTINATION "${USCITA}/chiaro")
set(PW "una frase lunga per i punti di prova")
file(WRITE "${USCITA}/pw" "${PW}\n")
execute_process(COMMAND "${KZERO}" chiudi "${USCITA}/chiaro" --scrivi "${USCITA}/chiuso" --utente TITOLARE --conservazione 2099-12-31
                        --password-da-ingresso
                INPUT_FILE "${USCITA}/pw" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero chiudi: ${r}")
endif()
file(REMOVE_RECURSE "${USCITA}/chiaro")
set(S "${ESEMPI}/k027-punti")
execute_process(COMMAND "${CREA}" --fogli "${USCITA}/k027-schede.xlsx" "Schede=${S}/schede.tsv" "Fermate=${S}/fermate.tsv" "Date=${S}/date.tsv"
                RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx schede: ${r}")
endif()
execute_process(COMMAND "${CREA}" "${S}/elenco.tsv" "${USCITA}/k027-elenco.xlsx" RESULT_VARIABLE r)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "crea_xlsx elenco: ${r}")
endif()
# La copia con le schede e l'elenco letti: la base delle prove dei punti.
file(COPY "${USCITA}/chiuso/" DESTINATION "${USCITA}/letto")
execute_process(COMMAND "${KZERO}" leggi-schede "${USCITA}/k027-schede.xlsx" "${USCITA}/letto" --profilo PS-CVA --contratto CT-SCU --data 2026-09-10
                        --riferimento "schede di settembre" --validita attivazione --scrivi "${USCITA}/letto" --utente TITOLARE --password-da-ingresso
                INPUT_FILE "${USCITA}/pw" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero leggi-schede: ${r}")
endif()
execute_process(COMMAND "${KZERO}" leggi-elenco "${USCITA}/k027-elenco.xlsx" "${USCITA}/letto" --profilo PR-CVA --anno-scolastico 2026-2027
                        --elenco completo --scrivi "${USCITA}/letto" --utente TITOLARE --password-da-ingresso
                INPUT_FILE "${USCITA}/pw" RESULT_VARIABLE r OUTPUT_QUIET)
if(NOT r EQUAL 0)
  message(FATAL_ERROR "kzero leggi-elenco: ${r}")
endif()
# La copia "punti": i punti della dimostrazione del mandato (conferma della proposta, punti a mano con le due volte di Lagobello, il sospeso
# sbloccato solo il mercoledi', chi non frequenta, la doppia tratta con il consenso, il cambio di fermata dal 01/12/2026).
file(COPY "${USCITA}/letto/" DESTINATION "${USCITA}/punti")
function(passo nome)
  cmake_parse_arguments(A "" "" "RIGHE;ARGOMENTI" ${ARGN})
  set(testo "${PW}\n")
  foreach(r IN LISTS A_RIGHE)
    string(APPEND testo "${r}\n")
  endforeach()
  file(WRITE "${USCITA}/ingresso-${nome}" "${testo}")
  execute_process(COMMAND "${KZERO}" ${A_ARGOMENTI} --utente TITOLARE --password-da-ingresso --scrivi "${USCITA}/punti"
                  INPUT_FILE "${USCITA}/ingresso-${nome}" RESULT_VARIABLE r OUTPUT_QUIET ERROR_VARIABLE e)
  if(NOT r EQUAL 0)
    message(FATAL_ERROR "passo ${nome}: ${r} ${e}")
  endif()
endfunction()
set(P "${USCITA}/punti")
passo(conferma-primaria RIGHE "U-000004 AM: SCH-4000903-1 / F-PRADELLO / 1" "U-000004 RM: SCH-4000904-1 / F-PRADELLO / 1"
      "U-000006 AM: SCH-4000903-1 / F-PRADELLO / 1" "U-000008 AM: SCH-4000903-1 / F-PRADELLO / 1" "U-000008 RM: SCH-4000904-1 / F-PRADELLO / 1"
      ARGOMENTI conferma-punti "${P}" --dal 2026-09-14 --riferimento "elenco di settembre" --data 2026-09-10)
passo(conferma-infanzia RIGHE "U-000002 AM: SCH-4000901-1 / F-PRADELLO / 1"
      ARGOMENTI conferma-punti "${P}" --dal 2026-09-21 --riferimento "elenco di settembre" --data 2026-09-10)
passo(rossi RIGHE "punto: SCH-4000901-1 / F-LAGOBELLO / 1"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000001 --tratta AM --dal 2026-09-21 --riferimento "mail della scuola del 15/9" --data 2026-09-15)
passo(mori RIGHE "punto: SCH-4000901-1 / F-LAGOBELLO / 2"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000007 --tratta AM --dal 2026-09-21 --riferimento "mail della scuola del 15/9" --data 2026-09-15)
passo(bruni RIGHE "punto: SCH-4000903-1 / F-PRADELLO / 1 / lunedi, martedi, mercoledi" "punto: SCH-4000903-1 / F-CASTELROVO / 1 / giovedi, venerdi"
      "consenso: mail del committente del 12/9"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000006 --tratta AM --dal 2026-09-14 --riferimento "mail del committente del 12/9" --data 2026-09-12)
passo(verdi-am RIGHE "punto: SCH-4000903-1 / F-CASTELROVO"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000003 --tratta AM --dal 2026-09-14 --riferimento "mail del committente del 12/9" --data 2026-09-12)
passo(verdi-rp RIGHE "punto: SCH-4000902-1 / F-CASTELROVO"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000003 --tratta RP --dal 2026-09-14 --riferimento "mail del committente del 12/9" --data 2026-09-12)
passo(sblocco-am ARGOMENTI sblocco "${P}" --utente-elenco U-000003 --tratta AM --giorni mercoledi --riferimento "mail del committente del 12/9"
      --data 2026-09-12)
passo(sblocco-rp RIGHE "chi: ufficio trasporti del committente"
      ARGOMENTI sblocco "${P}" --utente-elenco U-000003 --tratta RP --giorni mercoledi --a-voce --riferimento "telefonata del 12/9" --data 2026-09-12)
passo(neri RIGHE "motivo: famiglia" "nota: la famiglia lo accompagna"
      ARGOMENTI non-frequenta "${P}" --utente-elenco U-000004 --dal 2026-11-02 --riferimento "telefonata della famiglia del 30/10" --data 2026-10-30)
passo(viola RIGHE "punto: SCH-4000903-1 / F-CASTELROVO"
      ARGOMENTI assegna-punto "${P}" --utente-elenco U-000008 --tratta AM --dal 2026-12-01 --riferimento "mail del committente del 25/11" --data 2026-11-25)
