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
- 2026-10-11 - K-004 (chiusura): tolto dai prossimi passi il punto "prove automatiche", perche' fatto.
  In CLAUDE.md aggiunti il comando per eseguire le prove, la regola del software professionale
  e la regola dei codici mandato K-xxx.
- 2026-10-11 - K-005: prove portate su GoogleTest 1.15.2 (versione fissata), ogni prova si registra
  da sola; le 12 prove di prima sono rimaste identiche. Nuova prova: passeggero handicap che chiede
  l'accompagnatore = assistente obbligatorio; senza richiesta, da solo, no. 13 prove su 13 passate.
  Avvisi trattati come errori solo sul codice K-Zero (opzione KZERO_WERROR, usata dal controllo GitHub).
  Aperta la sezione "Bug e limiti noti".

## Bug e limiti noti
- check_home_breaks non usa ancora max_home_detour_km: oggi guarda solo la distanza da casa.
  Da completare con il calcolo vero (OR-Tools).
- I dati di prova sono copiati sia in src/main.cpp sia nelle prove: da unificare quando
  arrivera' il caricamento dei dati veri.

## Idee
- Generare scenari "gonfiati" (50-500 mezzi) dai giri reali per provare il carico.
- Chiedere i giri ad altre aziende del consorzio come secondo banco di prova.
- Indicatori: % km a vuoto e minuti fermi, per turno e per flotta.
- Spostare nel contratto altre regole del committente (mezzo minimo, pedana, tempi massimi).

## Prossimi passi
1. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
2. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
3. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
