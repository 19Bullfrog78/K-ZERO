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
- 2026-10-11 - K-006 (regole del mestiere, salvate direttamente su main): REGOLE.md aggiornato con
  attesa massima a scuola (arrivo non prima di 25 minuti dall'inizio, ripresa entro 20 minuti dalla fine),
  alunni attivi e sospesi (lo sblocco in un file separato, il file del CTA non si modifica mai), orari per
  giorno della settimana e periodo, turni del giorno e rotazione degli autisti, estate, servizi fissi,
  corse con piu' committenti; nuova sezione Glossario (GSH, MITT, inserviente, AM/RM/RP).
- 2026-10-11 - K-006 (programma, proposta "K-006: fondamenta uomo + AI"): un solo programma kzero con
  i comandi descrivi, versione e dimostrazione (la stampa di prova di prima, identica byte per byte, ora
  anche in JSON). Uscita testo o JSON con "schema" e "schema_versione"; schemi JSON Schema draft-07 in
  docs/schema; errori con codice fisso K-ERR (elenco in docs/errori.md); codici di uscita 0/1/2/3; nel
  JSON nessun dato personale salvo --includi-dati-personali. Codice diviso in nucleo (src/nucleo) e API
  (src/api), vedi docs/architettura.md. Nuove librerie CLI11 2.7.2 e nlohmann/json 3.12.0, con versione
  e impronta fissate. Prove: 63 su 63 (16 nucleo, 6 API, 24 sul programma, 1 documentazione,
  1 architettura, 15 sugli schemi con lo strumento standard check-jsonschema). Sabotaggio di 4 regole:
  tutte scoperte dalle prove. Controllo GitHub su ubuntu-24.04 con la verifica degli schemi.
- 2026-10-12 - K-007 (regole del mestiere, salvate direttamente su main): REGOLE.md riorganizzato in due parti.
  Parte 1, regole generali del mestiere: servizi e corse (la parola "linea" nei due sensi; di solito un servizio
  si perde o si scambia una corsa alla volta, a volte un intero lotto), orari fissi per le linee e liberi per
  scolastico e handicap, posti del libretto, mezzi e strade, trasbordi, assistente secondo regolamento o contratto,
  corse ibride autorizzate, dati personali nel lavoro (tre classi, due livelli). Parte 2: il regolamento della
  Provincia di Trento come esempio, con la fonte di ogni valore. L'attesa massima 25/20 non e' piu' una regola
  generale: e' un valore del regolamento di Trento. Due punti "da confermare": infanzia e medie insieme;
  "mezzi con piu' di 9 posti" letto come conducente compreso.
- 2026-10-13 - K-007 aggiunta 1 (regole del mestiere, salvate direttamente su main): REGOLE.md, posti a bordo.
  La carrozzina toglie posti e chiede la pedana solo per chi viaggia seduto sulla carrozzina; la carrozzina piegata
  e gli altri ausili dichiarati vanno nel bagagliaio, oppure tra i sedili (legati, bloccati, se i posti sono liberi)
  solo sui mezzi per cui l'azienda lo ammette. Gli ausili si contano, non si misurano. Il passeggino non si pianifica.

## Bug e limiti noti
- check_home_breaks non usa ancora max_home_detour_km: oggi guarda solo la distanza da casa.
  Da completare con il calcolo vero (OR-Tools).
- I dati di prova sono copiati sia in src/nucleo/dimostrazione.cpp sia nelle prove: da unificare quando
  arrivera' il caricamento dei dati veri.
- Le regole aggiunte con K-006 (attesa massima, attivi/sospesi, giorni e periodi, turni, rotazione,
  servizi fissi, piu' committenti) non sono ancora nel modello dati: entreranno con K-007 e con il calcolo.
- Il formato testo mostra sempre i dati personali (e' pensato per le persone): la protezione vale per il
  JSON, che e' il formato da usare per gli assistenti AI.
- Su Windows (K-009) sono da sistemare e provare gli argomenti con lettere accentate e gli a capo CR LF.

## Idee
- Generare scenari "gonfiati" (50-500 mezzi) dai giri reali per provare il carico.
- Chiedere i giri ad altre aziende del consorzio come secondo banco di prova.
- Indicatori: % km a vuoto e minuti fermi, per turno e per flotta.
- Spostare nel contratto altre regole del committente (mezzo minimo, pedana, tempi massimi).

## Prossimi passi
1. K-007: formato standard dei dati e controllore.
2. K-008: lettore del file alunni del CTA.
3. K-009: programma per Windows da scaricare (i dati veri restano sul PC di Mauro).
4. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
5. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
6. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
