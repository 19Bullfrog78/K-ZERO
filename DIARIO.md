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
- 2026-10-13 - K-007 (programma, proposta "K-007: formato dei dati (rete e servizi) e controllore"): prima meta'
  del formato standard dei dati (docs/formato-dati.md): pacchetto = cartella con manifesto e 13 file JSON (aziende,
  luoghi e tratti con limiti di accesso, regolamenti con la fonte di ogni valore, calendari, scuole e centri,
  committenti, contratti, gruppi di servizi, servizi con la domanda, corse con passaggi, trasbordi e coincidenze,
  mezzi, rimesse, tempi di sosta), uno schema per file in docs/schema, controllati con la libreria valijson 1.1.3.
  Tre classi di dati marcate negli schemi (comuni, personali, particolari) e due livelli di pacchetto ("solo numeri",
  "completo"): di base testo e JSON mostrano il codice al posto dei dati personali e "riservato" al posto dei dati
  particolari. Nuovo comando kzero controlla (46 controlli con codice K-CTR, catalogo in docs/controlli.md).
  Modello del nucleo esteso e riallineato (corse, passaggi, servizi e domanda al posto di Route, StopVisit,
  Passenger, Destination); ricerca dei luoghi per nome o codice esterno con normalizzazione documentata.
  Pacchetti esempi/demo e esempi/demo-numeri (dati inventati, regolamento di Trento con le fonti); kzero dimostrazione
  ora legge esempi/demo, incorporato nel programma (schema dimostrazione 2; descrivi 2). Orari oltre le 24:00
  restano tali anche in uscita. Prove: 242 su 242 (77 nucleo, 21 API, 86 sul programma, di cui 48 su una copia del demo
  con un solo valore cambiato, 1 documentazione, 1 architettura, 55 sugli schemi, 1 generazione della prova di scala).
  Le 63 prove di prima ci sono ancora, con lo stesso nome; adattate perche' usano il nuovo modello e i dati del demo.
  Sabotaggio: orario che torna indietro spento, margine dei trasbordi ignorato, attesa massima scritta nel codice
  (25 minuti), dati particolari sempre visibili: tutti scoperti (prove rosse: 3, 3, 2 e 7, piu' le prove sugli schemi
  di quelle uscite, non eseguite).
  Prova di scala (600 mezzi, 5.210 luoghi di cui 5.000 fermate, 2.000 servizi, 10.000 corse, 15 MB; generatore con
  seme fisso, file identici a ogni giro): kzero controlla senza errori ne' avvisi in 1,1-1,5 secondi con 129 MB di
  memoria (macchina di prova: 4 processori Xeon a 2,1 GHz). Non e' una soglia delle prove.

## Bug e limiti noti
- check_home_breaks non usa ancora max_home_detour_km: oggi guarda solo la distanza da casa.
  Da completare con il calcolo vero (OR-Tools).
- Turni del giorno, rotazione degli autisti e servizi fissi (regole aggiunte con K-006) non sono ancora nel
  modello dati: entreranno con K-008. Attesa massima, attivi/sospesi, giorni e periodi e corse con piu'
  committenti ci sono dal K-007.
- Le regole dei regolamenti (attese sulle corse, posti a bordo, assistente, passeggeri in piedi, limiti di accesso
  dei mezzi) sono nei dati ma non sono ancora verificate sui giri: arriveranno con la verifica delle regole,
  dopo il K-008.
- I nomi di scuole, centri, servizi e gruppi sono dati comuni ma testi liberi: un nome come "Centro diurno" lascia
  intuire un dato sulla salute. Chi scrive i dati deve evitare nomi che rivelano dati personali o particolari.
- Ricerca dei luoghi: le lettere di altri alfabeti (greco, cirillico...) non vengono ricondotte all'alfabeto latino.
- Ora legale: nei due giorni del cambio una corsa che attraversa le 02:00-03:00 dura un'ora in piu' o in meno di
  quanto risulta dagli orari (docs/formato-dati.md).
- Su Windows (K-009) sono da sistemare e provare gli argomenti con lettere accentate e gli a capo CR LF.

## Idee
- Generare scenari "gonfiati" (50-500 mezzi) dai giri reali per provare il carico.
- Chiedere i giri ad altre aziende del consorzio come secondo banco di prova.
- Indicatori: % km a vuoto e minuti fermi, per turno e per flotta.
- Scenari di variazione degli orari scolastici: mezzi e km risparmiati spostando l'inizio delle lezioni, come
  materiale per la richiesta al committente (le proposte vanno presentate entro il 28 febbraio, REGOLE.md).
- Ottimizzazione per zone: dividere il lavoro per elenchi di comuni (il comune di ogni luogo e' gia' obbligatorio).
- Consorzio con scambio di servizi tra aziende: prima chiarire chi autorizza, come si divide il risparmio, la
  riservatezza dei dati di ogni azienda.
- Limiti delle strade (altezze, masse, strade strette) letti dalle mappe aperte.
- Viaggi reali dalla piattaforma GPS del committente, per confrontarli con i giri pianificati.
- Trasbordi proposti dal programma (oggi si scrivono a mano e il programma li controlla).

## Prossimi passi
1. K-008: formato dei dati, parte 2 (autisti, abilitazioni, turni del giorno, giornata dell'autista e del mezzo,
   rotazioni, servizi fissi, blocchi).
2. Lettore del file alunni del CTA, che lo trasforma nel formato K-Zero.
3. K-009: programma per Windows da scaricare (i dati veri restano sul PC di Mauro).
4. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
5. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
6. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
