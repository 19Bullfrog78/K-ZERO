# Diario K-Zero

## Progressi
- 2026-10-11 - Creata l'ossatura: modello dati (passeggeri, destinazioni, mezzi, autisti, assistenti,
  ricoveri, contratti, corse, turni) e programma di prova. Compila senza errori ne' warning.
- 2026-10-11 - Aperto l'archivio con storico su GitHub (repository privato 19Bullfrog78/K-ZERO):
  primo salvataggio dell'ossatura su main. Compila senza errori ne' warning.
- 2026-10-11 - Tolta da CLAUDE.md la regola sulla cartella CODICE/ (l'archivio ufficiale e' solo GitHub)
  e tolto dai prossimi passi il punto "archivio con storico", perche' fatto.
- 2026-10-11 - Aggiunta in CLAUDE.md la sezione "Come si salvano le modifiche": documenti su main,
  programma e CLAUDE.md con proposta, controllo dell'arbitro e approvazione di Mauro.
- 2026-10-11 - Prove automatiche delle regole di REGOLE.md (tests/, 12 prove, eseguite con
  `ctest --test-dir build`): giri, posti, assistente, carrozzine e pedana, categorie di corsa,
  assistente per l'infanzia, pause. Controllo automatico su GitHub a ogni proposta e salvataggio
  su main: compila con i warning trattati come errori ed esegue le prove. 12 su 12 passate.

## Bug aperti
- (nessuno)

## Idee
- Generare scenari "gonfiati" (50-500 mezzi) dai giri reali per provare il carico.
- Chiedere i giri ad altre aziende del consorzio come secondo banco di prova.
- Indicatori: % km a vuoto e minuti fermi, per turno e per flotta.
- Spostare nel contratto altre regole del committente (mezzo minimo, pedana, tempi massimi).

## Prossimi passi
1. Test automatici sui casi gia' noti (es. 38 ragazzi su 16 posti = 3 giri).
2. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
3. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
4. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
