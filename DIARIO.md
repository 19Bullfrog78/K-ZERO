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
  mezzi, rimesse, tempi di sosta), uno schema per file in docs/schema, controllati con la libreria valijson 1.1.3;
  ogni numero ha un massimo di buon senso (un errore di battitura come 3000000000 persone viene rifiutato).
  Tre classi di dati marcate negli schemi (comuni, personali, particolari) e due livelli di pacchetto ("solo numeri",
  "completo"): di base testo e JSON mostrano il codice al posto dei dati personali e "riservato" al posto dei dati
  particolari. Nuovo comando kzero controlla (46 controlli con codice K-CTR, catalogo in docs/controlli.md).
  Modello del nucleo esteso e riallineato (corse, passaggi, servizi e domanda al posto di Route, StopVisit,
  Passenger, Destination); ricerca dei luoghi per nome o codice esterno con normalizzazione documentata.
  Pacchetti esempi/demo e esempi/demo-numeri (dati inventati, regolamento di Trento con le fonti); kzero dimostrazione
  ora legge esempi/demo, incorporato nel programma (schema dimostrazione 2; descrivi 2). Orari oltre le 24:00
  restano tali anche in uscita. Prove: 243 su 243 (77 nucleo, 21 API, 87 sul programma, di cui 49 su una copia del demo
  con un solo valore cambiato, 1 documentazione, 1 architettura, 55 sugli schemi, 1 generazione della prova di scala).
  Le 63 prove di prima ci sono ancora, con lo stesso nome; adattate perche' usano il nuovo modello e i dati del demo.
  Sabotaggio: orario che torna indietro spento, margine dei trasbordi ignorato, attesa massima scritta nel codice
  (25 minuti), dati particolari sempre visibili: tutti scoperti (prove rosse: 3, 3, 2 e 7, piu' le prove sugli schemi
  di quelle uscite, non eseguite).
  Prova di scala (600 mezzi, 5.210 luoghi di cui 5.000 fermate, 2.000 servizi, 10.000 corse, 15 MB; generatore con
  seme fisso, file identici a ogni giro): kzero controlla senza errori ne' avvisi in 1,1-1,5 secondi con 129 MB di
  memoria (macchina di prova: 4 processori Xeon a 2,1 GHz). Non e' una soglia delle prove.
- 2026-10-13 - K-007 aggiunta 1 (regole del mestiere, salvate direttamente su main): REGOLE.md, posti a bordo.
  La carrozzina toglie posti e chiede la pedana solo per chi viaggia seduto sulla carrozzina; la carrozzina piegata
  e gli altri ausili dichiarati vanno nel bagagliaio, oppure tra i sedili (legati, bloccati, se i posti sono liberi)
  solo sui mezzi per cui l'azienda lo ammette. Gli ausili si contano, non si misurano. Il passeggino non si pianifica.
- 2026-10-13 - K-007 aggiunta 1 (programma, stessa proposta K-007, correzioni dopo il controllo dell'arbitro):
  carrozzina "sulla_carrozzina" oppure "piegata"; ausili piegati dichiarati (es. deambulatore) per passeggero e per
  fermata (dati particolari); per ogni mezzo quanti ausili entrano nel bagagliaio e se l'azienda li ammette tra i
  sedili, con i posti occupati (dati comuni). La regola "ci sta sul mezzo" (fits) ora conta seduti, carrozzine,
  ausili piegati e assistente: prima il bagagliaio, poi tra i sedili se ammesso e se i posti sono liberi, altrimenti
  non ammesso. kzero dimostrazione ha la riga "Ausili piegati" (ALFA-04 auto con il baule, ALFA-02 minibus con
  l'ausilio tra i sedili). Nuovi controlli: K-CTR-047 (passeggero che ha bisogno dell'assistente in un gruppo che
  non lo richiede; di base il messaggio non nomina l'assistente) e K-CTR-048 (file non salvato in UTF-8: "ANSI" con
  riga e colonna, oppure UTF-16, con il rimedio e senza riportare il testo del file); K-CTR-031 confronta anche gli
  ausili piegati. Testo vuoto: un solo errore, senza "..". Demo: G-DIS chiede l'assistente (P-D1 ne ha bisogno),
  servizio del giovedi' con una carrozzina piegata in auto (S-DIS-GIO, corsa C-DIS-GIO); il nome della corsa non
  dice "carrozzina" (sarebbe un dato sulla salute in un campo comune). Schemi modificati nella versione attuale
  (servizi 1, mezzi 1, dimostrazione 2). Prove: 259 su 259 (83 nucleo, 24 API, 92 sul programma, di cui 54 su una
  copia del demo con un solo valore cambiato, 1 documentazione, 1 architettura, 57 sugli schemi, 1 generazione della
  prova di scala). Sabotaggio: ausilio tra i sedili che non occupa posti, ausili sempre ammessi, bagagliaio ignorato,
  messaggio di K-CTR-047 senza filtro: tutti scoperti (prove rosse: 6, 6, 6 e 1).
- 2026-10-18 - K-008 (regole del mestiere, salvate direttamente su main): REGOLE.md, "Turni del giorno e
  rotazione": ogni azienda sceglie la sua rotazione (es. 3, 8 o 14 giorni) e il suo modo di assegnare i turni;
  chi fa il turno in tre modi (rotazione, servizi fissi con i loro giorni, assegnazione del singolo giorno scritta
  dall'utente), vince il piu' preciso; turno scoperto se nessuno lo fa; la rotazione di 14 giorni di Alpinabus
  (senza nomi) come esempio. "Autisti": la pausa a casa vale dai 30 minuti in su se il giro lo consente (risposta
  di Mauro del 13/10), il rientro dev'essere giustificato dai dati, la regola precisa la fissa il mandato "orari di
  lavoro"; il mezzo dorme preferibilmente vicino a casa dell'autista (impostazione dell'azienda: preferito,
  ammesso, mai); tolta dai "Da confermare" la domanda sulla pausa con il mezzo o con l'auto. "Mezzi e strade": un
  mezzo fa solo i servizi per cui e' omologato; per l'altro uso serve la distrazione, per singolo mezzo e con le
  date (Codice della strada artt. 82 e 87, circolare n. 27690 del 27/11/2013).
- 2026-10-18 - K-008 (programma, proposta "K-008: formato dei dati (persone e turni)"): seconda meta' del formato
  standard dei dati (docs/formato-dati.md), 10 file nuovi (norme, festivita, persone, assenze, fermi, turni, rotazioni,
  servizi_fissi, assegnazioni, blocchi) e versioni nuove di 4 file (calendari 2, regolamenti 2, mezzi 2, servizi 2); la
  versione 1 si legge ancora (provato con una copia congelata del demo del K-007). Elenchi e impostazioni di ogni azienda
  in aziende.json (tipi di attivita', di assenza, di fermo, competenze, norme del paese, ricovero vicino a casa,
  abbinamento autista-mezzo): nessun valore di un'azienda nel codice. Festivita' come dati e calendari che si combinano
  (aggiungono o tolgono altri calendari e festivita'). Persone con ruoli, disponibilita', abilitazioni con scadenza,
  autorizzazioni del committente, prestiti; requisiti di guida dalle norme del paese (nessuna patente nel codice). Turno
  del giorno come unita' di base (un mezzo, corse intere o a pezzi con il cambio dell'autista, attivita'); chi fa il turno
  in tre modi (assegnazione del singolo giorno, servizio fisso, rotazione: vince il piu' preciso; se la persona prevista
  non puo', turno scoperto); uso dei mezzi, distrazioni e vincoli di destinazione; blocchi. Pause a casa senza valori nel
  codice: senza regola nei dati la funzione non giudica. Nuovo comando kzero giornata (giornata di tutti, di una persona
  o di un mezzo in una data: turni, corse con passaggi, carichi e scarichi, km pieni e a vuoto, giorni nel periodo; vale
  come scheda scuola del committente); controlla 2 (giorni dei problemi raggruppati in periodi), descrivi 3, nuovi errori
  K-ERR-013 ... K-ERR-018, 39 controlli nuovi (K-CTR-049 ... K-CTR-087). Pacchetti esempi/demo ed esempi/demo-numeri con
  persone e turni (dati inventati). Prove: 457 su 457 (144 nucleo, 33 API, 152 sul programma, di cui 65 su una copia del
  demo con dati cambiati apposta e 3 sulla prova di scala, 1 documentazione, 1 architettura, 125 sugli schemi,
  1 generazione della prova di scala); nuove prove di robustezza (file costruiti per rompere il programma, ogni numero e
  testo degli schemi con un massimo). Le 259 prove di prima ci sono ancora; adattate quelle che contano i controlli, le
  versioni delle uscite o i dati del demo. Sabotaggio (contate anche le prove sugli schemi di quelle uscite, non
  eseguite): sovrapposizione a cavallo della mezzanotte ignorata, autista senza abilitazione mai segnalato,
  assegnazione del singolo giorno che non vince piu', calendari e festivita' tolti ignorati, mezzo senza l'uso richiesto
  sempre accettato, nome delle persone sempre visibile: tutti scoperti (prove rosse: 4, 4, 39, 70, 4 e 16; per l'uso
  dei mezzi all'inizio una sola prova, poi aggiunte due prove sul programma).
  Prova di scala (aggiunte 1.320 persone, 1.200 turni del giorno, una rotazione per azienda da 3 a 28 giorni e una a
  passi di un mese, un anno di festivita', assenze coperte): kzero controlla senza errori ne' avvisi in 9,2 secondi con
  130 MB (la parte del K-007 da sola 1,7 secondi); kzero giornata di tutti in 11,1 secondi con 247 MB, di una persona
  in 9,4 secondi con 131 MB (rifa' tutti i controlli). Macchina di prova: 4 processori Xeon a 2,8 GHz. Non e' una
  soglia delle prove.

- 2026-10-19 - K-009 (programma, proposta "K-009: sicurezza di base"): **lettura dei file blindata** (solo file normali;
  collegamenti seguiti solo se portano a un file normale dentro la cartella del pacchetto, anche se la cartella e' a sua
  volta un collegamento; mai piu' di 256 MB, leggendo fino al massimo piu' uno anche se il sistema non dice la dimensione;
  una cartella con il nome di un file e' un errore proprio; nuovi K-CTR-088, 089, 090; un byte zero o un numero
  troppo grande sono un K-CTR-003). **Messaggi e posizioni**: ogni controllo dichiara cosa rivela (nessun dato,
  personale, particolare; anche variante per variante); di base i problemi sui dati sulla salute sono una riga sola con
  quanti errori e avvisi sono; le posizioni non entrano mai in un dato personale o particolare (regola ricavata dagli
  schemi; un'assenza diventa la persona); controlla 3 con i problemi riservati, la posizione completa con il parametro
  e quanti dati personali e particolari ci sono nel pacchetto; vale anche per giornata ed errori K-ERR. **Luoghi
  sensibili** (luoghi versione 2; il centro diurno del demo): con il nome del passeggero e senza i dati particolari il
  legame con il luogo e' riservato. **Niente rete** provato sul programma compilato e sui sorgenti. **Controllo GitHub**
  con tre lavori: prove normali, sanitizer (AddressSanitizer e UndefinedBehaviorSanitizer, clang 18, tutte le prove) e
  fuzzing (libFuzzer, 27 bersagli: ogni tipo di file del pacchetto, la riga di comando, date e orari; 20 secondi
  ciascuno). **Protezioni del compilatore** nella versione di rilascio per Linux. **Elenco dei componenti**
  docs/componenti.cdx.json (CycloneDX 1.5). Nuovo docs/sicurezza.md; principi e comandi in CLAUDE.md.
  Prove: 489 su 489 (144 nucleo, 42 API, 167 sul programma, di cui 4 sulla prova di scala, 1 documentazione,
  1 architettura, 179 sugli schemi, 4 di sicurezza, 3 rigiocate dal fuzzing; alcune prove hanno due etichette); con i
  sanitizer 487 su 487 (senza le 2 prove sul programma di rilascio: protezioni e simboli di rete). Le 457 prove di prima
  ci sono ancora; adattate quelle che si aspettavano una posizione dentro un dato personale (es. /persone/2/abilitazioni/2
  diventa /persone/2) o un problema sui dati sulla salute elencato di base (ora contato nella riga riassuntiva),
  l'uscita attesa della dimostrazione con i dati personali (luogo sensibile) e il conteggio dei controlli.
  Fuzzing in locale (4 processori Xeon a 2,1 GHz): tre giri su tutti i 27 bersagli, 60, 600 e 120 secondi per
  bersaglio (l'ultimo sul codice finale), circa 5 ore e 50 minuti di fuzzing in tutto: 2.013.232 file del pacchetto
  deformati, 636.687 righe di comando, 34.376.180 date e orari. Guasti trovati e corretti:
  1. fuzzing, fermi.json: un numero oltre ogni misura (es. 1e400) faceva uscire il programma con un errore interno
     (K-ERR-009, codice 3) che ricopiava il numero letto dal file; ora e' un K-CTR-003 con riga e colonna, e il caso e'
     una prova normale (tests/fuzz/casi);
  2. preparando un sabotaggio: un file valido seguito da un byte zero e da altro testo era letto a meta' (la libreria JSON
     prende lo zero per la fine del file); ora e' un K-CTR-003, e il fuzzing controlla che non succeda piu';
  3. AddressSanitizer: nelle prove dei turni del K-008 un puntatore a un turno di un elenco gia' distrutto (uso di
     memoria liberata, prove verdi per caso); ora un elenco temporaneo e' rifiutato gia' alla compilazione;
  4. clang: una lambda che catturava una structured binding (ammesso solo da C++20) e una costante non usata nelle
     prove; gcc 13 con i sanitizer da' un falso avviso dentro <regex>, per questo i sanitizer usano clang.
  Sabotaggio (prove rosse; tra parentesi le prove sugli schemi di quelle uscite, non eseguite): lettura oltre il massimo
  2; file non normale accettato 2; problema sulla salute elencato di base con codice e posizione 5 (1); posizione dentro
  un dato personale mostrata di base 15 (3), compresi i casi rigiocati dal fuzzing; luogo sensibile mostrato con il nome
  del passeggero 3 (1); una funzione di rete aggiunta apposta (socket) 2. Tutti scoperti.
  Prova di scala: kzero controlla 4,3 secondi con 132 MB, kzero giornata di tutti 5,2 secondi con 249 MB (macchina di
  prova: 4 processori Xeon a 2,1 GHz). Non e' una soglia delle prove.
- 2026-10-19 - K-009 aggiunta 1 (programma, nella proposta "K-009: sicurezza di base"): **case dei passeggeri e luoghi
  sensibili**. L'arbitro ha trovato il buco: con --includi-dati-personali la giornata di ALFA-03 e la corsa C-DIS-1 della
  dimostrazione mostravano "PP-01 Casa Bianchi" e poi il centro diurno. Ora una regola sola (LegameSensibile, in
  src/api/riservatezza) per tutte le uscite: con i dati personali e senza quelli particolari, dove si riconosce una
  persona trasportata (il nome di un passeggero o la sua casa) i luoghi sensibili diventano "riservato". Insieme:
  l'intera giornata mostrata (giornata, tre viste), servizi e corse (dimostrazione), il messaggio (controlla); il perche'
  in DECISIONI.md (nel demo T-AUTO prende Casa Neri e consegna il trasbordo T-01 che T-DIS porta al centro: un turno da
  solo non bastava). Sostituisce le due regole del punto 3, scritte a parte. Schemi delle uscite alla stessa versione.
  Prove: 516 su 516 (27 nuove: la regola da sola, i messaggi, la giornata in tre viste in testo e JSON, inizio e fine in
  un luogo sensibile, una corsa con una casa e senza luogo sensibile, un luogo sensibile senza case, la dimostrazione);
  cambiate due righe dell'uscita attesa della dimostrazione con i dati personali (il centro nelle corse C-DIS-1 e
  C-DIS-GIO). Con i sanitizer 514 su 514. Fuzzing in locale 60 secondi per bersaglio sui 27 bersagli (i file del
  pacchetto ora anche con i soli dati personali): 115.065 file deformati, 42.216 righe di comando, 2.608.998 date e
  orari, nessun guasto. Sabotaggio (prove rosse; tra parentesi quelle sugli schemi di quelle uscite, non eseguite):
  regola spenta per le sole attivita' del turno 10 (4), 5 in testo e 5 in JSON; regola spenta del tutto 15 (5), 6 in
  testo e 9 in JSON o nell'API. Tutti scoperti.

## Bug e limiti noti
- Pause a casa: la regola (dai quanti minuti, entro quanti km da casa, deviazione massima) non e' ancora nei dati;
  senza regola check_home_breaks non giudica. La deviazione massima non e' ancora usata. La regola la scrivera' il
  mandato "orari di lavoro". I pesi del calcolo (ObjectiveWeights) hanno ancora valori provvisori nel codice, non usati:
  andranno nei dati con il mandato del calcolo.
- Il regolamento di esempio R-TN cita il testo unico 1.B.1 come fonte dell'attesa 25/20, ma per i servizi speciali la
  fonte e' la Carta della mobilita' 2013 (DGP 2649/2012, punto 5), che fissa anche la durata a bordo di regola entro
  40 minuti, non ancora nel regolamento di esempio: da sistemare con il mandato sulla verifica delle regole.
- Le regole dei regolamenti (attese sulle corse, posti a bordo, assistente, passeggeri in piedi, limiti di accesso
  dei mezzi) sono nei dati ma non sono ancora verificate sui giri: arriveranno con la verifica delle regole,
  dopo il K-008.
- I nomi di scuole, centri, servizi, gruppi, corse, turni, calendari, le note e i codici sono dati comuni ma testi liberi:
  un nome come "Centro diurno" lascia intuire un dato sulla salute. Chi scrive i dati deve evitare nomi che rivelano dati
  personali o particolari (docs/sicurezza.md, con esempi). Il demo, scritto prima, ne ha (S-DIS, "Centro diurno,
  andata"): restano perche' i dati sono inventati e le prove li usano.
- Ricerca dei luoghi: le lettere di altri alfabeti (greco, cirillico...) non vengono ricondotte all'alfabeto latino.
- Ora legale: nei due giorni del cambio una corsa che attraversa le 02:00-03:00 dura un'ora in piu' o in meno di
  quanto risulta dagli orari (docs/formato-dati.md). Vale anche per i turni: nella notte del 25/10/2026 un turno
  che attraversa le 02:00-03:00 (es. una reperibilita' notturna) dura un'ora in piu' di quanto dicono inizio e fine. Da tenere presente nel
  mandato "orari di lavoro".
- Su Windows (mandato del programma per Windows) sono da sistemare e provare gli argomenti con lettere accentate e gli a
  capo CR LF, e da provare la lettura blindata (pipe, dispositivi, collegamenti e scorciatoie, cartelle sincronizzate come
  OneDrive con i "file su richiesta") e le protezioni del compilatore equivalenti (/GS, /guard:cf, /DYNAMICBASE,
  /HIGHENTROPYVA, /NXCOMPAT): oggi sono provate solo su Linux.
- Sicurezza (docs/sicurezza.md): i file dei pacchetti sono in chiaro sul disco, con un solo utente e senza registro degli
  accessi, fino al mandato K-010 "cassaforte dei dati personali". Il filtro dei luoghi sensibili chiude il legame dentro
  un'uscita (K-009 aggiunta 1), non tra uscite diverse: chi puo' usare --includi-dati-personali puo' anche chiedere la
  stessa giornata senza, dove il centro si vede accanto al codice della casa (lo chiudono utenti e permessi, K-010);
  restano anche i nomi liberi del demo (il turno "Centro diurno", il servizio "Trasporto al centro diurno Il Faro", che
  nel JSON della giornata nomina ancora il centro) e il legame tra messaggi diversi di controlla. Un file vicino ai 256 MB preparato apposta puo' occupare diversi GB di memoria
  mentre lo si legge. Un collegamento fisso (hard link) non si distingue da un file normale, e chi ha gia' accesso al PC
  puo' cambiare un file mentre lo si legge. Le librerie del sistema (C e C++) non sono nell'elenco dei componenti; che
  l'elenco rispetti lo schema CycloneDX e' controllato solo in locale, non nelle prove.
- K-CTR-018 (fermata ambigua) puo' essere troppo severo con i dati veri: fermate con lo stesso nome sui due lati
  della strada, "Bahnhof" in ogni paese. Da rivedere con il lettore GTFS.
- kzero giornata rifa' tutti i controlli del pacchetto a ogni richiesta, anche per una sola persona: con la prova di
  scala servono circa 9 secondi. Accettato per ora (un risultato su dati sbagliati sarebbe peggio); da rivedere se
  l'attesa diventa scomoda nell'uso vero.
- Di base il messaggio di K-CTR-068 (autista senza l'abilitazione) non dice quale abilitazione serve: insieme al codice
  della persona direbbe quale patente non ha, che e' un dato personale. Si vede con --includi-dati-personali.
- Il fuso orario del manifesto (versione 1, gia' su main) non ha una lunghezza massima nello schema: e' l'unica eccezione
  alla prova dei limiti di buon senso. Si sistema con la prossima versione del manifesto.
- Le regole sugli orari di lavoro degli autisti (ore al giorno e alla settimana, riposi, pause) non sono ancora nei
  dati ne' controllate: i turni si controllano solo per sovrapposizioni, abilitazioni, disponibilita' e assenze.

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
- (Mauro, 13/10) Ricerca del mezzo adatto per un servizio nuovo o ipotetico: dati orario ed esigenze (posti,
  carrozzine, ausili piegati, assistente), proporre i mezzi in ordine di convenienza (capienza, pedana,
  bagagliaio, dove si trova il mezzo a quell'ora, km a vuoto, regole sull'orario di lavoro degli autisti, es. oltre
  9 posti su un servizio di oltre 50 km cambiano le regole della settimana). Possibile dopo i turni (K-008), le
  distanze e la verifica delle regole.
- Distribuzione dei lavori extra (gite, noleggi) a rotazione o per anzianita', secondo la scelta dell'azienda.
- Scelta dei turni da parte degli autisti (preferenze espresse, poi il programma propone e l'azienda decide).
- Servizi a chiamata (prenotati il giorno prima o in giornata), da inserire nei turni gia' fatti.

## Prossimi passi
1. K-010: "cassaforte dei dati personali": dati personali cifrati con una password, piu' utenti, registro degli
   accessi (docs/sicurezza.md), prima di qualunque dato vero.
2. Mandato "orari di lavoro": regole degli orari degli autisti, pausa a casa, filosofia aziendale nei dati.
3. Lettore del file alunni del CTA, che lo trasforma nel formato K-Zero (con la lettura blindata del K-009).
4. Programma per Windows da scaricare (i dati veri restano sul PC di Mauro), con le prove di sicurezza su Windows.
5. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
6. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
7. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
