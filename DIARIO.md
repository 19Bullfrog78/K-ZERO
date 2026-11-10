# Diario K-Zero

## Progressi
- 2026-10-11 - Creata l'ossatura: modello dati (passeggeri, destinazioni, mezzi, autisti, assistenti,
  ricoveri, contratti, corse, turni) e programma di prova. Compila senza errori ne' warning.
- 2026-10-11 - Aperto l'archivio con storico su GitHub (repository privato 19Bullfrog78/K-ZERO):
  primo salvataggio dell'ossatura su main. Compila senza errori ne' warning.
- 2026-10-11 - Tolta da CLAUDE.md la regola sulla cartella CODICE/ (l'archivio ufficiale e' solo GitHub)
  e tolto dai prossimi passi il punto "archivio con storico", perche' fatto.
- 2026-10-11 - Aggiunta in CLAUDE.md la sezione "Come si salvano le modifiche": documenti su main,
  programma e CLAUDE.md con proposta, controllo dell'arbitro e approvazione del supervisore.
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
  alunni attivi e sospesi (lo sblocco in un file separato, il file del consorzio non si modifica mai), orari per
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
  dall'utente), vince il piu' preciso; turno scoperto se nessuno lo fa; la rotazione di 14 giorni di ALP
  (senza nomi) come esempio. "Autisti": la pausa a casa vale dai 30 minuti in su se il giro lo consente (risposta
  del supervisore del 13/10), il rientro dev'essere giustificato dai dati, la regola precisa la fissa il mandato "orari di
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
- 2026-10-20 - K-010 (programma, proposta "K-010: cassaforte dei dati personali"): **cassaforte** (`cassaforte.kzc` nella
  cartella del pacchetto, formato byte per byte in docs/cassaforte.md, versione 1, con una copia congelata nelle prove):
  tutti i valori personali e particolari, scelti con la stessa regola delle classi che li conta, legati al loro elemento
  per codice (o per la chiave dichiarata nello schema, es. turno e data di un'assegnazione del giorno), mai per
  posizione; un valore senza il suo elemento, o con piu' elementi possibili, e' un errore (K-CTR-094), un valore anche
  in chiaro pure (K-CTR-095). Manifesto versione 2 con "cassaforte" (mancante K-CTR-091, non dichiarata K-CTR-092, di
  un'altra chiusura K-CTR-093) e fuso orario di al massimo 64 caratteri; dati in chiaro senza "dati_inventati" K-CTR-096;
  data di conservazione passata K-CTR-097. **Cifratura** con libsodium 1.0.22: XChaCha20-Poly1305; Argon2id con 256 MiB
  e 3 passaggi (circa mezzo secondo per apertura: 0,47-0,53 secondi su 5 misure, Xeon a 2,1 GHz); una coppia di chiavi
  X25519 per utente e chiavi nuove consegnate in busta (crypto_box_seal) quando si toglie un utente; tre chiavi (base,
  personali, particolari) e una fessura per utente con le sole chiavi dei suoi permessi; intestazione autenticata con la
  chiave base, elenchi di chi ha i dati personali o particolari autenticati con quelle chiavi (risposta 1 dell'arbitro).
  **Password**: mai sulla riga di comando ne' in una variabile d'ambiente; dal terminale senza eco (la nuova due volte)
  oppure dall'ingresso standard con --password-da-ingresso; almeno 15 caratteri e fino a 256, normalizzate NFC
  (utf8proc 2.11.3), rifiutate quelle comuni (elenco SecLists); nessun recupero. **Comandi nuovi**: chiudi, richiudi,
  esporta, cassaforte, registro, aggiungi-utente, togli-utente, password, conservazione, cancella; --utente per
  controlla e giornata; --includi-dati-* solo entro i permessi (K-ERR-023); i controlli che hanno bisogno di una classe
  che l'utente non puo' aprire escono come "non verificati"; nome e casa di chi va in un luogo sensibile sono dati
  particolari in tutte le uscite e il luogo si vede sempre (DECISIONI.md, punto 5 a e b). **Registro degli accessi**
  (`registro.kzr`, cifrato, voci a catena, ultima voce ricordata dalla cassaforte): prima la voce, poi i dati; scritture
  atomiche e blocco della cartella. Uscite: controlla 4, giornata 2, descrivi 4, cassaforte 1, registro 1; errori
  K-ERR-019 ... 038; controlli K-CTR-091 ... 097.
  Prove: 679 su 679 (144 nucleo, 45 API, 57 di sicurezza di cui 53 interne della cassaforte, 273 sul programma
  di cui 3 sulla prova di scala, 154 sugli schemi, 3 rigiocate dal fuzzing, 1 documentazione, 1 architettura, 1
  generazione della prova di scala); 163 nuove, tra cui: la copia congelata della cassaforte versione 1
  (tests/esempi/k010-cassaforte) aperta dai tre tipi di utente in ogni uscita, testo e JSON; ogni byte cambiato,
  troncata, vuota, di un altro pacchetto, versione sconosciuta, parametri abbassati o assurdi, password sbagliata, utente
  sconosciuto; chiave pubblica sostituita, utente aggiunto da fuori, utente tolto, permesso alzato a mano, busta finta
  (risposta 1 f dell'arbitro); unione per codice dopo riordini e cancellazioni; "non verificato" confrontato su 375
  pacchetti; interruzioni a meta' della chiusura, della richiusura e del registro (processo fermato di colpo); nove
  comandi insieme sulla stessa cartella; dati decifrati mai su disco. Le 516 prove del K-009 ci sono ancora (una sola
  rinominata, stesso caso: giornata_casa_senza_luogo_sensibile, ora giornata_casa_legata_su_una_corsa_senza_centro);
  adattate quelle dei luoghi sensibili alla regola nuova (casa e nome di chi va al centro con il codice, il centro
  visibile), il numero dei controlli (97) e la tabella dei comandi (una scelta obbligatoria non ha un valore
  predefinito). Con i sanitizer 677 su 677 (senza le 2 prove sul programma di rilascio), 13 minuti e mezzo con 4 prove
  insieme: le tre prove del "non verificato" durano circa 12 minuti l'una, per questo nel controllo GitHub le prove dei
  sanitizer ora girano in parallelo.
  Fuzzing in locale (4 processori Xeon a 2,1 GHz), due giri: i 5 bersagli nuovi 15 minuti ciascuno, poi
  tutti i 32 bersagli 2 minuti ciascuno sul codice finale, circa 2 ore e 20 minuti in tutto: 1.016.270 cassaforti
  deformate, 602.408 contenuti della cassaforte, 9.943.456 registri deformati e 6.844.219 elenchi di voci, 5.092.454
  password, piu' 67.049 file del pacchetto, 32.183 righe di comando e 5.631.188 date e orari nel giro finale. Nessun
  guasto. Il bersaglio "registro" prova solo l'intestazione e la cifratura (un registro deformato non si decifra mai):
  la lettura delle voci e la catena le prova "registro-voci", sul testo gia' decifrato. Il fuzzing del controllo GitHub
  compila anche rigioca_fuzz, che prepara i semi della cassaforte con chiavi nuove a ogni giro (al primo giro sulla
  proposta mancava e il lavoro si fermava subito).
  Sabotaggio (prove rosse): --includi-dati-* accettati senza il permesso 4; valori decifrati scritti in un
  file temporaneo 1 (prova nuova: dati decifrati mai su disco); voce del registro non scritta e dati mostrati 1;
  intestazione non controllata 3; elenchi dei permessi non controllati (chiave pubblica sostituita, permesso alzato a
  mano) 2; intestazione ed elenchi non controllati insieme (anche l'utente aggiunto da fuori) 7; catena del registro non
  controllata 1; utente tolto senza chiavi nuove 4; unione per posizione invece che per codice 3; regola dei luoghi
  sensibili tolta dalle uscite 19 (anche cosi' l'utente con i soli dati personali non legge i nomi: nella cassaforte
  stanno con i particolari); password di 8 caratteri accettata 1; blocco della cartella tolto 1. Tutti scoperti.
- 2026-10-26 - K-010 aggiunta 1 (programma, nella proposta "K-010: cassaforte dei dati personali", dopo il controllo
  dell'arbitro del 26/10). **1. Dati legati a un luogo sensibile solo con tutti e due i permessi.** Nome, casa,
  indirizzo e coordinate di chi va in un luogo sensibile stanno in una terza sezione della cassaforte, "dei legati",
  cifrata con una chiave ricavata dalle chiavi dei personali e dei particolari insieme (HKDF-SHA256 di libsodium; scelta
  e motivi in DECISIONI.md): prima stavano con i particolari, e chi aveva i soli particolari li decifrava. A ogni cambio
  di una delle due chiavi la sezione si ricifra, quindi togliere un utente ora lo fa solo chi ha tutti e due i permessi.
  Formato ancora versione 1 (non e' su main): copia congelata tests/esempi/k010-cassaforte rifatta, stessi utenti e
  password. Sulla copia del demo (prova interna Cassaforte.CopiaCongelataDelDemoVociPerUtente, solo file e campo):
  UFFICIO (soli personali) decifra 106 voci di 23 campi, SALUTE (soli particolari) 35 voci di 9 campi, nessuno dei due le
  24 voci legate (luoghi.json nome, indirizzo, comune, coordinate; servizi.json nome e casa; 4 ciascuno), nemmeno
  provando sul file tutte le chiavi che si procura; TITOLARE tutte e tre le sezioni. **2. Cassaforte finta** fatta da
  chi ha solo i file, con un registro riscritto con la chiave base finta: ogni utente la rifiuta (K-ERR-028), nessun dato
  (Accesso.CassaforteFintaFattaSenzaPassword). **3. Registro riscritto prima dell'ancora** (stesse voci, catena
  ricalcolata, una voce cambiata): K-ERR-029, nessun dato (Accesso.RegistroRiscrittoPrimaDellAncora). **4. Avvisi sulle
  password**, testi nel catalogo dei controlli, uguali in testo e JSON: K-CTR-098 (chiavi nuove ricevute in una busta:
  ogni apertura lo ricorda, in ogni comando, finche' l'utente non cambia la password), K-CTR-099 (dopo togli-utente),
  K-CTR-100 (password scritta da un altro: dopo aggiungi-utente e password --per-utente). Cambiare la password, anche per
  se', da' ora una coppia X25519 nuova (DECISIONI.md). La data di conservazione passata usa ovunque il testo del catalogo
  (K-CTR-097). Uscite: cassaforte 1 e registro 1 con "avvisi" {codice, messaggio}, giornata 2 con cassaforte.avvisi
  (schemi cambiati sul posto: non sono su main); controlli K-CTR-098 ... 100.
  Prove: 701 su 701, 22 nuove (5 interne: legati sul file anche dopo un cambio delle chiavi di ciascun livello, copia
  del demo, cassaforte finta, registro riscritto, avvisi sulle password in sequenza; 12 sul programma e 5 sugli schemi:
  ogni avviso in ogni comando, togliere un utente con un solo permesso). Cambiate: Unione.AssenzeIntereEPasseggeriLegati
  (chi ha i soli particolari non legge piu' il nome di P-D1; aggiunto chi ha tutti e due i permessi); lo strumento di
  test_unione da' i legati solo con tutti e due i permessi, come la cassaforte; Cassaforte.PermessoAlzatoAManoRifiutato
  (togliere un utente vuole tutti e due i permessi); Controlli.catalogo_coperto (100 controlli);
  programma.cassaforte_conservazione_passata_giornata (testo del catalogo); piu' testi cercati in cinque prove sul
  programma della cassaforte. Nessuna prova tolta.
  Con i sanitizer 699 su 699 (senza le 2 prove sul programma di rilascio), 16 minuti e mezzo con 3 prove insieme.
  Fuzzing in locale sul codice finale, i 32 bersagli 2 minuti ciascuno, 4 alla volta: 135.093 cassaforti deformate
  (con la sezione dei legati), 124.143 contenuti della cassaforte, 992.588 registri e 685.694 elenchi di voci, 546.974
  password, 67.538 file del pacchetto, 28.779 righe di comando, 4.274.162 date e orari. Nessun guasto.
  Sabotaggio (prove rosse), quelli dell'arbitro: valori legati cifrati con la sola chiave dei particolari 57 (le 2 prove
  interne sul file; le altre perche' la copia congelata non si apre piu': le uscite da sole non l'avrebbero scoperto,
  perche' il programma non decifra i legati per chi ha un solo permesso); variante con la stessa chiave della sezione
  dei particolari 57; controllo delle storie tolto 1 (la cassaforte finta si apriva per tutti e tre gli utenti, giornata
  con uscita 0); impronta dell'ancora non confrontata 1. In piu': legati non ricifrati al cambio delle chiavi 5; avviso
  K-CTR-098 mai mostrato 13; stessa coppia X25519 cambiando la propria password 1. Rifatti sul codice nuovo i 12 del
  K-010: --includi-dati-* senza permesso 4, valori in un file temporaneo 1, voce del registro non scritta 1,
  intestazione non controllata 3, elenchi non controllati 2, tutti e due 7, catena 1, utente tolto senza chiavi nuove
  17, unione per posizione 3, LegameSensibile tolto dalle uscite 19, password di 8 caratteri 1, blocco tolto 1. Tutti
  scoperti.
- 2026-10-27 - K-011 (programma, proposta "K-011: orari di lavoro"): **orari di lavoro degli autisti**. Tempi di ogni
  persona giorno per giorno per categoria (guida, altre mansioni, disponibilita', reperibilita', interruzione, riposo),
  dalle categorie scritte dall'azienda (aziende.json versione 2: `attivita[].tempo`, `soste_non_scritte`); versione 1
  leggibile con prudenza (K-CTR-101). **Leggi come dati** (norme.json versione 2): regimi con quando valgono (ruoli,
  rapporti, contratti collettivi, tipi di servizio, persone trasportabili con il conducente, km del percorso), esclusioni,
  estensione alla settimana (nota INL 61/2021), esclusioni tra regimi, servizi occasionali, casi non trattati, cambi
  dell'ora; 11 tipi di regola, ognuno con la lettura certa e, se serve, la prudente; legge certa = errore, da confermare
  = avviso con il motivo, senza dati mai "regolare". Demo: R-L138, R-561, R-234, R-CHIAMATA ed ESEMPIO-01 come al punto
  3 del mandato (REGOLE.md, "Orari di lavoro", con le fonti e l'elenco del "da confermare" per il consulente del lavoro).
  **Contratto della persona** (persone.json versione 2: ore previste e massime), **scelte dell'azienda e della persona**
  (`scelte_orari`, preferenza o limite), **chiave del rientro** (supervisore, 20/10: fermo minimo, fattore della guida, km in
  piu', forza; tolti HomeBreakRule, check_home_breaks e il peso della pausa). **`kzero ore`** (schema `ore` versione 1;
  K-ERR-039 e 040). **Controlli K-CTR-101 ... 126**; gli errori del piano degli orari non bloccano kzero giornata e
  kzero ore (schemi controlla 5, giornata 3, descrivi 5). Demo e demo "solo numeri" alla versione 2 (Alfa: fermo 30
  minuti, fattore 2, preferenza; Beta: 45 minuti, fattore 1,5, 15 km, limite; P-E con il suo fermo; P-D con il suo
  lavoro massimo; P-G a chiamata con 200 giornate dal 2023): 0 errori, 31 avvisi (prima 8). Per restare senza errori
  ho corretto un turno, non una regola: la gita di domenica 15 novembre porta il 561 su tutta la settimana di P-A, che
  senza il riposo regolare di 45 ore lo violava; ora venerdi' 13 il primo lo fa P-C (assegnazione del singolo giorno).
  Pacchetto di prova tests/esempi/k011-orari (dati inventati, generato dal demo): i tre esempi della pausa del supervisore, una
  violazione per tipo di regola, la corsa di 52 km con mezzi da 17, 8 e 9 posti, una corsa senza km, il titolare, il
  lavoro a chiamata, il contratto collettivo, le ore del contratto, due autisti a bordo: 15 errori (tutti del piano), 89
  avvisi. Prova di scala alla versione 2, con le leggi del demo: turni rifatti perche' le rispettino anche nella lettura
  prudente (corse piu' corte, una pausa di almeno 45 minuti a meta' turno, chi fa il pomeriggio non fa il mattino dopo):
  0 errori, 1 avviso (la media su 4 mesi del D.Lgs. 234 che esce dal periodo).
  Trovati e corretti strada facendo: un riposo di 36-41 ore a meta' settimana, in una settimana con il riposo regolare,
  veniva preso per un riposo settimanale ridotto da compensare (6 avvisi falsi nel demo, 84 nella scala); una giornata
  senza corse faceva scattare l'estensione del 561 a tutta la settimana; una scelta dell'azienda sull'ampiezza riportava i
  motivi delle leggi; uscita di kzero ore troppo grande (i dati delle corse ripetuti per ogni regime) e troppa memoria
  (il documento intero e tre copie dell'uscita): ora una persona alla volta, le corse una volta sola al giorno.
  Sabotaggio (uno alla volta, il codice rotto apposta; prove rosse), gli 11 del mandato: soste fino a 30 minuti non
  contate 1 (Orari.soste_di_30_e_di_31_minuti); estensione del 561 solo al giorno 18; persone del mezzo senza il
  conducente 4; interruzione 30 + 15 accettata 2; riposi ridotti non contati tra due settimanali 1; "da confermare"
  trattato come certo 30; come assente 18; finestra prima del periodo contata come zero (nessuna verifica parziale) 15;
  valore della persona ignorato 1; rientro con il diretto a zero tra luoghi diversi 1; limite rigido su un'assegnazione del
  singolo giorno dato come errore 1. Tutti scoperti. Con i sanitizer 796 prove su 796, 30 minuti con 3 prove insieme
  (le tre del "non verificato" sui 375 pacchetti ora sono le piu' lunghe). Fuzzing in locale sul codice finale, i 32
  bersagli 60 secondi ciascuno, 4 alla volta (ogni file deformato ora passa anche dal resoconto degli orari, quindi i
  casi al minuto sono meno di prima): 19.754 file del pacchetto, 7.313 righe di comando, 1.100.884 date e orari, 33.025
  cassaforti e 29.262 contenuti, 271.992 registri e 192.037 elenchi di voci, 166.562 password. Nessun guasto. Prove: 798
  su 798 sul programma finale (prima del K-011 701); quelle cambiate e il perche' sono elencate nella proposta. Tempi
  della prova di scala: in "Bug e limiti noti".
- 2026-10-28 - K-011 aggiunta 1 (programma, nella proposta "K-011: orari di lavoro", dopo il controllo dell'arbitro e
  la decisione del supervisore del 28/10 su OUT e tachigrafo). **Ore del contratto e scelte sul lavoro**: i conteggi
  possibili sono quelli dei regimi certi, di quelli da confermare e, se nessuno e' certo, quello senza regime; nella
  lettura certa il piu' basso, nella prudente il piu' alto: errore solo se tutti superano (caso dell'arbitro: P-A con
  2,1 ore il 14 ottobre, prima errore K-CTR-119, ora avviso K-CTR-120 con il motivo). **D.Lgs. 234 nei giorni estesi
  del 561**: da confermare, con il motivo (`estensione_come_regime`, di base "da_confermare"). **Guida delle corse
  escluse** = altre mansioni per le regole di guida del 561 nella lettura certa, guida in quella prudente con un
  avviso (`guida_delle_corse_escluse`; Reg. 561 art. 6 c. 5, TRACE II, ANAV n. 5); i trasferimenti a vuoto seguono la
  corsa a cui servono. **OUT o tachigrafo** (aziende.json versione 2, `modo_corse`): scelta per tipo di servizio,
  servizio o corsa (vince la piu' precisa); tachigrafo = il 561 (e il 234) per scelta, con la sua forza e i suoi
  codici (K-CTR-127, 128), anche per l'estensione; OUT dove la legge vuole il 561 non vale (K-CTR-132); scelte diverse
  sulla stessa corsa (K-CTR-133); dichiarazione (K-CTR-131) e forzatura cosciente (K-CTR-129, rischio K-CTR-130,
  avvisi "forzatura dell'azienda"), sempre visibili in controlla, giornata e ore. Esclusioni con `insieme_a`: dal
  consulente lo scolastico fino a 50 km e' escluso con certezza, anche con disabili a bordo; il solo disabili resta da
  confermare. **Persona in prestito**: scelte della sua azienda, soste e rientro dell'azienda del turno (documentato,
  con una prova). Demo: Alfa linee e scolastico OUT, gite con il tachigrafo; Beta linee con il tachigrafo
  (preferenza). Controllo del demo: 0 errori, 31 avvisi come prima; cambia solo il conteggio della media di 4 mesi del
  D.Lgs. 234 (K-CTR-104, persone 11 -> 5), perche' lo scolastico ora e' escluso con certezza. Pacchetto di prova: 11
  errori e 103 avvisi (prima 15 e 89): 4 errori di guida di P-561-17 diventano avvisi (guida delle corse escluse);
  avvisi nuovi per il 234 nei giorni estesi (3), forzatura, rischio e dichiarazione (3), OUT che non vale (3),
  P-DISABILI (solo disabili, da confermare) e P-FORZATA; sparisce quello di P-GUIDA (scolastico ora escluso). Prova di
  scala: 0 errori, 0 avvisi (prima 1: la media del 234, che ora non vale per nessuno: la scala ha solo servizi
  scolastici). Sabotaggi dell'arbitro che non erano scoperti, ora con le loro prove (rosse con il codice rotto): 50 km
  esatti con ">=" (programma.ore_50_km_esatti_esclusi); due settimane senza il regolare tolto
  (Orari.due_riposi_settimanali_ridotti_di_fila); soste non scritte al limite 30/31 con ">="
  (Orari.soste_non_scritte_di_30_e_di_31_minuti); due regimi certi, il piu' alto invece del piu' basso
  (Orari.contratto_con_due_regimi_certi_il_conteggio_piu_basso). Sabotaggi nuovi (prove rosse; tutti, anche quelli
  dell'arbitro, rifatti sul codice finale): ore del contratto senza i regimi da confermare 2; il piu' alto tra i
  regimi certi 1; 234 che non segue l'estensione 5; estensione del 234 sempre certa 7; guida esclusa come guida nella
  lettura certa 10; trasferimenti non legati alle corse 3; scelta del singolo servizio ignorata 5; OUT accettato dove
  la legge vuole il 561 senza forzatura 14; OUT senza forzatura trattato come forzato 13; forzatura senza avviso 6;
  dichiarazione senza nota 4; rischio della forzatura non registrato 5; forzatura come esclusione certa 5; tachigrafo
  per scelta ignorato 4. Tutti scoperti. Prove di prima cambiate: controlla_orari,
  ore/giornata_errori_del_piano_non_bloccano (numeri), ore_52_km_tutta_la_settimana ("Errore K-CTR-113" -> "Avviso
  K-CTR-114", resta "Errore K-CTR-117"), scala_controlla (1 avviso -> 0), il numero dei controlli del catalogo (126 ->
  133), tre giornate attese (solo la riga nuova "modo" delle corse). Prove: 834 su 834 (prima dell'aggiunta 798); con
  i sanitizer 832 su 832 (29 minuti, 3 prove insieme). Fuzzing in locale sul codice finale, i 32 bersagli 60 secondi
  ciascuno, uno alla volta: 22.761 file del pacchetto, 16.911 righe di comando, 2.215.593 date e orari, 77.156
  cassaforti e 74.347 contenuti, 576.836 registri e 366.716 elenchi di voci, 288.369 password. Nessun guasto.
- 2026-11-03 - K-011 aggiunta 2 (programma, ultima aggiunta, nella proposta "K-011: orari di lavoro", dopo il
  controllo dell'arbitro dell'aggiunta 1 a c8aea44). **Blocco con i soli dati particolari** (bloccante): con il
  pacchetto di prova chiuso, chi aveva solo i dati particolari (SAL) finiva kzero controlla, giornata e ore con
  K-ERR-009 ("key 'motivo' not found"): motivo e chi della forzatura e della dichiarazione ora si leggono come gli
  altri campi che possono mancare. Comandi divisi in lettura e lavoro su un pacchetto gia' letto; prova nuova che
  legge ogni pacchetto delle prove come lo ricevono chi ha solo i personali, chi ha solo i particolari e chi li ha
  tutti e lancia i tre comandi (mai un errore interno), con la copertura dei campi riservati degli schemi (il
  pacchetto di prova ora li ha tutti: forzatura su un servizio, dichiarazione su una corsa, nota di una persona,
  codici esterni e nota di un luogo privato, assenza di qualche ora); prova con la cassaforte vera (TIT, UFF, SAL).
  **Dichiarazione dato particolare** (decisione dell'arbitro), con la regola generale "campo personale dentro un dato
  particolare = legati" (motivo e chi: tutti e due i permessi). **Modo delle corse e tipo di servizio**: senza i dati
  particolari ogni corsa senza una scelta propria o del suo servizio ha la stessa riga "per un dato particolare"
  (prima la corsa per disabili del demo non aveva riga); con i dati particolari anche "nessuna scelta, decide la
  legge"; K-CTR-132 variante 4. **Regimi per scelta riservati tutti** dove ci sono scelte del tachigrafo per tipo
  (domanda fatta prima di scrivere codice; risposta dell'arbitro: riga fissa "Regimi seguiti per scelta dell'azienda:
  riservati", niente casi a meta'; K-CTR-127/128 varianti particolari). **Testi veri**: K-CTR-129 e 131 senza
  affermazioni false; K-CTR-134 e 135 quando forzatura o dichiarazione non cambiano niente. **Prova mancante** del
  tachigrafo come limite nella settimana estesa (sabotaggio dell'arbitro, chosen_weeks). Trovato strada facendo:
  K-CTR-129 era nascosto a chi aveva solo i particolari (segnato "personale" per il motivo): ora la forzatura si vede
  a tutti, con motivo e chi riservati. Semi della cassaforte per il fuzzing dal pacchetto di prova degli orari (legati
  anche dalle dichiarazioni). Demo: 0 errori, 31 avvisi (invariato). Pacchetto di prova: 11 errori, 107 avvisi (prima
  103). Prove di prima cambiate: tre giornate attese e giornata_mezzo_dati_personali (righe del modo),
  ore_tachigrafo_per_scelta_beta e ore_dichiarazione_con_una_nota (ora con --includi-dati-particolari),
  ore_forzatura_motivo_riservato, testi di K-CTR-129 in due prove, controlla_orari (K-CTR-131 non piu' di base; 129,
  134 in piu'), 103 -> 107 avvisi, catalogo 133 -> 135, Api.classi_marcate_negli_schemi,
  Orari.tachigrafo_su_una_linea_fino_a_50_km (variante 13 -> 43). Prove: 867 su 867 (prima dell'aggiunta 2: 834); con
  i sanitizer 865 su 865. Fuzzing in locale sul codice finale, i 32 bersagli 60 secondi ciascuno: 24.341 file del
  pacchetto, 16.172 righe di comando, 2.342.133 date e orari, 75.956 cassaforti e 47.036 contenuti, 578.101 registri e
  415.867 elenchi di voci, 313.458 password; nessun guasto. Sabotaggi sul codice finale (prove rosse): motivo letto di
  nuovo con at 5; dichiarazione comune 2; personale dentro un particolare non legato 5; dichiarazione nominata senza i
  dati particolari 2; forzatura nascosta a chi ha solo i particolari 2; corsa senza riga del modo quando manca la
  scelta del tipo 4; regime per scelta non riservato 3; K-CTR-127/128 senza varianti particolari 1; K-CTR-132 scelte
  diverse tra tipi comune 1; K-CTR-129 con il testo di prima 3; avvisi "non serve" mai scritti 8; forzatura da
  confermare presa per utile 1; forza del tachigrafo non estesa alla settimana 1. Tutti scoperti.
- 2026-11-04 - K-012 (programma, proposta "K-012: tipi di servizio NCC e promiscuo, clienti privati"). **1. Il tipo NCC**
  (noleggio con conducente: gite, turismo, eventi) in ogni elenco dei tipi: servizi 3, norme 3, regolamenti 3, mezzi 3,
  aziende 3; uscite dimostrazione 3 e ore 2. Il tipo dice la natura del servizio, non la licenza del mezzo (una navetta di
  operai con tragitto e orari fissi resta "altro"). Le versioni vecchie si leggono ancora e "altro" resta "altro". Categoria
  della corsa: per un servizio NCC i gruppi non la cambiano (una gita scolastica e' NCC); piu' servizi = promiscuo anche con
  un NCC. Norme 3: servizi occasionali tipo per tipo (demo: l'NCC certo, "altro" da confermare, con la fonte); usi per
  servizio: l'NCC chiede l'uso NCC con certezza, "altro" da confermare; Alfa sceglie il tachigrafo per l'NCC. Trovato strada
  facendo: il trasferimento a vuoto dalla rimessa non seguiva la corsa NCC per l'interruzione di 15 + 15 minuti dei servizi
  occasionali (errore falso): ora la segue, come per la guida delle corse escluse. **2. "Ibrido" diventa "promiscuo"** in
  uscite, messaggi, descrivi, documenti, descrizioni degli schemi in uso, prove e CLAUDE.md; le righe vecchie di DECISIONI e
  DIARIO restano (storia: fino al K-011 la categoria si chiamava "ibrido"); l'alimentazione "ibrido" dei mezzi resta.
  **3. Clienti privati**: committenti.json versione 2 con `committenti_privati` (nome e nota personali, codice e codici
  esterni comuni), mai nel "solo numeri" (K-CTR-019, varianti 8 e 9); `LegameSensibile` esteso ai privati (gruppi, fermate
  o passeggeri in un luogo sensibile, corse che ci passano anche con i trasbordi, case porta a porta con il porta a porta
  sensibile), una regola sola anche per la cassaforte. Su domanda (risposta: "nome nelle uscite") giornata versione 4 e
  dimostrazione 3 mostrano il nome del committente, filtrato; il supervisore: il nome non si perde mai, chi ha i permessi lo vede
  sempre (REGOLE.md, docs/cassaforte.md). **4. Prova di scala mista** (DECISIONI.md): 610 mezzi, 9.800 corse (40 linee oltre i
  50 km, 400 corse promiscue), NCC con pullman e auto, disabili verso 20 centri sensibili, OUT o tachigrafo per tipo e per
  servizio, 20 prestiti di persone e 10 di mezzi; 0 errori, 1 avviso (K-CTR-104: la media di 4 mesi del D.Lgs. 234 esce
  dal periodo, perche' ora il 561 vale per molte persone). Attese cambiate: scala_controlla "10000 corse, 600 mezzi ... 0
  errori, 0 avvisi" -> "9800 corse, 610 mezzi ... 0 errori, 1 avviso"; scala_giornata resta "1200 turni". Tempi sulla stessa
  macchina (4 processori, 15 GB; memoria = picco del processo), main -> K-012: controlla 10,5 -> 13,1 s (496 -> 498 MB); ore di
  una persona per l'anno 10,5 -> 13,1 s; di tutti per una settimana 11,9 -> 14,2 s; per un mese 14,8 -> 16,6 s (642 -> 663
  MB); per l'anno in testo 40,0 -> 43,9 s (1,7 -> 1,9 GB di memoria, 171 -> 243 MB di uscita), in JSON 72,1 -> 70,9 s (3,7
  -> 3,8 GB, 1,65 -> 1,81 GB di uscita); giornata di tutti 12,1 -> 14,5 s. Nessun tempo raddoppia. **5. Demo**: la gita e'
  NCC; due clienti privati inventati: CP-01 (matrimonio, sabato 2027-07-10, calendario con la data aggiunta) e CP-02
  (noleggio verso il centro diurno, mercoledi' 2027-07-07, nome legato), fatti da P-G (a chiamata) con il suo minibus
  ALFA-06 per un'assegnazione del singolo giorno; demo "solo numeri" senza i nomi. Il demo resta 0 errori, 31 avvisi
  (uguali): cambiano due conteggi, K-CTR-104 (persone 5 -> 6: P-G nei giorni dei noleggi ha il 561 e il 234) e K-CTR-109 (P-G
  375 -> 377 giornate a chiamata, 6 -> 8 giorni). Valori: 257 personali e 37 particolari -> 262 e 43 (nomi e nota dei
  privati, note delle due assegnazioni; tipi e gruppi dei servizi nuovi; tolta la descrizione del tipo "altro" della gita).
  Pacchetti di prova: tests/esempi/k011-orari resta alla versione 2 (stesso esito: 11 errori, 107 avvisi); nuovo
  tests/esempi/k012-servizi (genera_k012.py: il pacchetto degli orari rifatto sul demo di oggi piu' gita scolastica NCC,
  auto NCC da 8 posti, pullman NCC, navetta "altro", turni dei privati): 11 errori (del piano, quelli del K-011), 107 avvisi
  (due in piu' rispetto al K-011, K-CTR-107 dell'auto NCC e K-CTR-114 della navetta; due in meno, K-CTR-084 e 085, perche'
  P-G e ALFA-06 ora lavorano). **6. Documenti**: REGOLE.md (categorie, NCC, NCC e leggi sugli orari, domanda 11, clienti
  privati), docs/formato-dati.md, docs/controlli.md, docs/cassaforte.md, docs/architettura.md, docs/sicurezza.md,
  DECISIONI.md, CLAUDE.md. Nessun codice K-CTR o K-ERR nuovo (servivano solo due varianti di K-CTR-019).
  Prove: 906 su 906 (prima del K-012: 867), 39 nuove: categorie NCC e promiscuo (nucleo e pacchetto dei servizi), servizi
  occasionali tipo per tipo e nelle versioni, legame dei committenti privati (gruppo, corsa che passa dal centro, trasbordo,
  porta a porta, enti mai), privati nei messaggi e nella cassaforte, demo chiuso con TIT, UFF e SAL in giornata (testo e
  JSON), dimostrazione con i privati, pacchetto k012 (controlla, NCC con pullman e auto, navetta "altro", gita scolastica),
  scala (linea lunga, auto NCC). Prove di prima cambiate: categorie (Hybrid -> Mixed, la gita NCC), conteggi del demo (servizi, corse,
  valori, K-CTR-019 37 -> 42), versioni lette del demo, testi delle giornate e della dimostrazione con il nome del
  committente (4 uscite attese rigenerate: cambiano solo le righe dei committenti e i servizi nuovi), la prova dei 15 + 15
  minuti (ora sulla gita NCC), scala_controlla. Nessuna prova tolta.
  Con i sanitizer 904 su 904 (senza le 2 prove sul programma di rilascio), 25 minuti con 4 prove insieme (compilazione 6
  minuti).
  Fuzzing in locale sul codice finale, i 32 bersagli 60 secondi ciascuno, 4 alla volta, con i semi anche del pacchetto
  dei servizi (k012) e la cassaforte preparata da quel pacchetto (privati legati): 16.605 file del pacchetto, 15.493 righe di
  comando, 2.035.952 date e orari, 71.105 cassaforti e 27.031 contenuti, 527.280 registri e 358.362 elenchi di voci,
  291.735 password. Nessun guasto.
  Sabotaggi sul codice finale, uno alla volta, ricompilato, con tutte le prove (prove rosse): NCC che diventa scolastico
  con un gruppo di studenti 5; NCC trattato come "altro" da confermare per il 561 3; trasferimento a vuoto non legato alla
  corsa NCC 5; norme 2 con il "da confermare" dei servizi occasionali ignorato 1; "ncc" letto come "altro" 18; "ibrido"
  rimesso in uscita 4; nome del privato comune 20; privato di un servizio verso un luogo sensibile non legato 1; privato con
  una corsa che passa da un luogo sensibile non legato 1; nome del committente non filtrato 8; nome e nota del privato nel
  "solo numeri" 1; nome del privato legato a chi ha solo i dati personali nei messaggi: 0 al primo giro, quindi la prova
  nuova Api.committente_privato_nei_messaggi, poi 1; linee oltre i 50 km escluse dal 561 27 (anche la scala); committenti
  privati non letti 123; formato-dati senza il riferimento a committenti 2: 1. Tutti scoperti.
- 2026-11-04 - K-013 (programma, proposta "K-013: orari senza i dati particolari e prove mancanti"). **1. Orari senza i dati
  particolari** (decisione dell'arbitro, DECISIONI.md): senza i dati particolari, per permesso o per parametro, tutto cio' che
  negli orari dipende dal tipo di servizio delle corse e' riservato: i regimi che ne dipendono (dai dati: tipi di servizio,
  esclusioni, servizi occasionali, come_regime, esclude; il regime per scelta se ne dipende quello di legge o se la scelta e'
  per tipo), quando valgono e perche', "Non valgono", estensioni, lavoro contato, notturno, tipo dei riposi, verifiche
  parziali, il lavoro per il contratto e "senza regime" (se uno di loro conta il lavoro), "nessuna legge" (se uno di loro e'
  una legge sugli orari) e i problemi, che si contano soltanto (`Problem::by_service_kind`, `dal_tipo` nel catalogo; tolte le
  varianti +30 di K-CTR-127 e 128). Niente casi a meta': in kzero ore una riga fissa per persona (schema ore 3,
  `regimi_dal_tipo_di_servizio`), senza i codici dei regimi; mai piu' "Problemi degli orari: nessuno" senza i dati
  particolari. Domanda fatta prima di scrivere il codice (cassaforte: le due richieste del mandato non stavano insieme),
  risposta "uguali sul tipo": il titolare senza il parametro legge "riservato", chi ha i soli dati personali "non verificato"
  (anche nella riga dei regimi per scelta). **Prova per differenza** (tests/test_unione.cpp): 30 coppie (le 10 coppie di
  tipi su tre servizi: escursione NCC con il pullman, disabili del centro diurno fino a 50 km, noleggio con l'auto da 9
  persone), tutte senza errori nei dati, nessuna differenza fuori dai totali; con i dati particolari ogni coppia di tipi
  cambia l'esito delle leggi in almeno un servizio. Con il programma di main le stesse coppie avevano uscite diverse in 28
  su 30 (le due che no, NCC e "altro" nel centro diurno e con l'auto, hanno lo stesso esito delle leggi). Trovati
  fuori dagli orari e non corretti (domanda per l'arbitro): K-CTR-068 e K-CTR-069 dicono il tipo di servizio senza i dati
  particolari (in "Bug e limiti noti"). **2. Persona in prestito**: Gamma (inventata, nel pacchetto dei servizi) senza scelte
  del tachigrafo per tipo; P-GAMMA-PRESTITO in prestito a Beta ha la riga fissa e il regime per scelta riservato, P-GAMMA no;
  chosen_by_kind resta (DECISIONI.md). **3. Prove mancanti dei servizi occasionali**: corsa promiscua NCC + scolastico nei due
  ordini, trasferimento che non si sa a quale corsa serve vicino a una corsa NCC, norme 2 senza da_confermare o con false.
  **4. Una funzione sola** per la corsa a cui serve un trasferimento a vuoto (`served_trip`), uscite identiche. **5.
  Documenti**: DECISIONI.md (al posto della nota del K-011 aggiunta 2), docs/controlli.md, docs/cassaforte.md,
  docs/architettura.md, CLAUDE.md. Nessun codice K-CTR o K-ERR nuovo.
  Demo: 0 errori, 31 avvisi (uguali); di base 13 avvisi ora si contano soltanto (prima 0). k011-orari: 11 errori e 107 avvisi
  (uguali), di base 8 errori e 27 avvisi contati soltanto. k012-servizi: 11 errori, 108 avvisi (uno in piu': K-CTR-045, Gamma
  senza mezzi), di base 8 errori e 29 avvisi contati soltanto. Prove: 929 su 929 (prima del K-013: 906), 23 nuove. Prove di
  prima cambiate: le prove del calcolo delle leggi che guardavano le leggi senza i dati particolari ora le chiedono
  (--includi-dati-particolari: 52 km, 50 km, 50,1 km, senza km, guida delle corse escluse, D.Lgs. 234 nei giorni estesi,
  titolare, forzatura, forzatura che non serve, errori del piano, controlla_orari_dati_personali, k012_controlla,
  scala_controlla), con il testo delle corse che ora dice il tipo; le prove della riservatezza seguono la regola nuova
  (settimana della gita, controlla_orari, controlla_non_serve_di_base, 15 + 15 "altro", tachigrafo per tipo, riga fissa,
  rientri giustificati, i conteggi "Problemi sui dati particolari" del demo e di tre pacchetti rotti, la variante +30 di
  K-CTR-128, la tabella di docs/controlli.md); k012_controlla 107 -> 108 avvisi. Nessuna prova tolta.
  Sabotaggi sul codice finale, uno alla volta, ricompilato, con tutte le prove (prove rosse): riga di un regime che dipende
  dal tipo visibile 12; segno dei problemi ignorato nel catalogo 14; segno mai messo dal nucleo 14; lavoro per il contratto
  visibile 8; "nessuna legge" visibile: 0 al primo giro, quindi il terzo servizio della prova per differenza e due prove sul
  programma, poi 4; come_regime non seguito 11; esclude non seguito: 0 al primo giro (nel demo il regime escluso
  dipende gia' dal tipo), quindi la prova del nucleo criterio per criterio, poi 1; K-CTR-104 senza il segno 9;
  K-CTR-132 senza il segno: 0 al primo giro (nessuna scelta OUT per corsa dove vale il 561), quindi due prove, poi
  1; riga fissa solo per chi lavora 1; "nessuno in questo periodo" 2; "riservato" a chi non ha il permesso 1;
  regime per scelta che dipende dal tipo solo con le scelte per tipo 1; riga del prestito tolta (M4) 3; certezza piu' alta
  tra i servizi di una corsa 1; trasferimento che non si sa trattato come certo 1; norme 2 senza da_confermare lette da
  confermare 1; la funzione sola cambiata in un punto 14 (rosse le prove di tutte e due le regole). Tutti scoperti.
  Con i sanitizer 927 su 927 (senza le 2 prove sul programma di rilascio), 2 prove insieme; al primo giro AddressSanitizer ha trovato un uso di memoria liberata nella prova per
  differenza (le corse lette da un resoconto gia' distrutto: un errore della prova, non del programma), corretto.
  Fuzzing in locale sul codice finale, i 32 bersagli 60 secondi ciascuno, 2 alla volta, con i semi anche del pacchetto dei
  servizi rigenerato: 19.166 file del pacchetto, 15.931 righe di comando, 2.051.694 date e orari, 67.793 cassaforti e 37.265
  contenuti, 543.762 registri e 361.217 elenchi di voci, 343.263 password. Nessun guasto. Tempi della prova di scala non
  rimisurati: il calcolo non cambia, cambia solo cosa si mostra.
- 2026-11-05 - Regola del supervisore, registrata in CLAUDE.md (nella proposta del K-013, perche' CLAUDE.md passa da una proposta):
  ogni resoconto per l'arbitro e ogni documento utile al progetto si consegna sempre come file da scaricare; il supervisore lo mette
  nella cartella giusta del suo PC (i resoconti in C:\Users\BULLFROG\Desktop\LAVORO\K-ZERO\K-ZERO CC). Una sessione nel
  cloud non raggiunge il PC: il resoconto del K-013 e' stato consegnato da scaricare.
- 2026-11-05 - K-014 (programma, proposta "K-014: requisiti per tipo senza i dati particolari"). **1. Requisiti che dipendono
  dal tipo di servizio** fuori dagli orari, dai dati e mai dal nome (`kind_requirements`, src/nucleo/turni.hpp): usi dei mezzi
  per tipo (regolamenti e norme), autorizzazione del committente per tipo, vincolo di destinazione con tipi di servizio,
  condizioni dei regolamenti con tipi di servizio (oggi nessuna uscita le usa). Cercando chi legge il tipo fuori dagli orari
  trovati anche la categoria della corsa (gia' particolare in kzero dimostrazione) e lo schema dei servizi (sotto). **2. Senza i
  dati particolari** (per permesso o per parametro) si contano soltanto, con il segno del K-013 (`by_service_kind`, `dal_tipo`):
  K-CTR-068 dell'autorizzazione del committente, K-CTR-069, K-CTR-072 con un vincolo per tipo, K-CTR-067 varianti 4 e 6. Scelta A
  del supervisore (5/11): se il pacchetto ha almeno un requisito che dipende dal tipo, di ogni turno scoperto o senza mezzo per un fatto
  con le date si vedono il fatto, i giorni e la persona o il mezzo previsto, il perche' no, con lo stesso testo e la stessa
  posizione per ogni causa (K-CTR-078 variante 5, K-CTR-079 variante 3, kzero giornata, kzero ore; `motivo_dei_turni_riservato`
  in src/api/riservatezza.hpp); lo stesso turno scoperto per cause diverse e' un problema solo; per chi non ha il permesso dei
  dati particolari la giornata dice "il motivo non e' verificato". Con i dati particolari tutto come prima. Trovata e chiusa
  una fuga in piu': "manca il campo obbligatorio 'tipo_descrizione'" (K-CTR-007) diceva il tipo "altro"; ora un campo
  obbligatorio particolare che manca si conta soltanto (variante 1, dalla classe del campo nello schema). **3. Prova per
  differenza estesa** (tests/test_unione.cpp), con i requisiti veri del demo: S-SCU-AM, S-DIS, S-LIN-B610, le 10 coppie di tipi
  ciascuno; 20 coppie della prova (con i dati particolari cambiano i requisiti o il piano; elenco atteso nella prova), 10 in
  piu' come controllo; differenze ammesse in DECISIONI.md (totali; il piano e quello che ne segue), ricerca dei testi e delle
  posizioni delle cause. Nessuna fuga; con il programma di main fughe in tutte le 20 coppie (14 nel confronto, le 6 di
  S-LIN-B610 con la ricerca delle cause). **4. Prova mancante del K-013 (B2)**: norme con il solo Reg. 561 come legge che
  dipende dal tipo, con regole solo su guida e riposi: "nessuna legge vale" riservato. **5. Documenti**: DECISIONI.md,
  docs/controlli.md, docs/sicurezza.md (rischio che resta), docs/cassaforte.md, docs/architettura.md, CLAUDE.md. Nessun codice
  K-CTR o K-ERR nuovo (varianti nuove di K-CTR-007, 078, 079).
  Demo: 0 errori, 31 avvisi, di base 13 contati soltanto (uguali). k011-orari: 11 errori e 107 avvisi; k012-servizi: 11 errori e
  108 avvisi (uguali). Prove: 961 su 961 (prima del K-014: 929), 32 nuove. Prove di prima cambiate: quelle che si aspettavano
  di base K-CTR-069 (mezzo senza l'uso richiesto) o il motivo di un turno scoperto o senza mezzo (assenza, fermo, demo con i
  dati personali, giornata della persona assente), ora con la versione di base e quella con i dati particolari. Nessuna prova
  tolta.
  Con i sanitizer 959 su 959 (senza le 2 prove sul programma di rilascio); le tre prove per differenza nuove con i sanitizer
  durano da 3 a 7 minuti ciascuna (49 secondi la piu' lunga senza). Per farle girare in locale con clang serve il pacchetto
  libclang-rt-18-dev anche per i sanitizer, non solo per il fuzzing (qui mancava: AddressSanitizer non si collegava).
  Fuzzing in locale sul codice finale, i 32 bersagli 60 secondi ciascuno, 2 alla volta: 19.445 file del pacchetto, 15.457 righe
  di comando, 2.505.643 date e orari, 71.806 cassaforti e 36.169 contenuti, 555.537 registri e 370.954 elenchi di voci, 312.778
  password. Nessun guasto. Nessun file di dati nuovo o cambiato: i semi sono quelli di prima.
  Sabotaggi sul codice finale, uno alla volta, ricompilato, con tutte le prove (prove rosse): K-CTR-068 dell'autorizzazione del
  committente visibile di base 5; K-CTR-069 visibile 5; K-CTR-072 con un vincolo per tipo visibile 4; K-CTR-067 varianti 4 e 6
  visibili 1; il perche' del turno senza mezzo visibile in kzero giornata 6; la causa di K-CTR-078 visibile con i dati personali
  7; il testo senza causa solo per le cause del tipo e non per il fermo 2 (e in kzero giornata solo per l'uso del mezzo 2); un
  requisito del punto 1 non riconosciuto (l'autorizzazione del committente) 1; B2 (law solo dalle regole sul lavoro) 1, prima 0;
  K-CTR-007 del campo particolare che manca visibile 1; il testo senza motivo anche senza requisiti del tipo 1; la posizione del
  fatto tenuta nel testo senza motivo 8. Tutti scoperti.

## Bug e limiti noti
- I pesi del calcolo (ObjectiveWeights) hanno ancora valori provvisori nel codice, non usati: andranno nei dati con il
  mandato del calcolo. Dal K-011 non c'e' piu' il peso della pausa (la pausa si giudica con la chiave del rientro); con
  il calcolo nessun peso dovra' premiare i km o la fretta (art. 10 del Reg. 561/2006).
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
  quanto risulta dagli orari (docs/formato-dati.md). Dal K-011 le date del cambio sono nei dati del paese
  (`norme[].cambi_ora`) e un turno che lo attraversa da' un avviso (K-CTR-103): le durate degli orari di lavoro di quel
  turno vanno controllate a mano, il programma non le corregge.
- Su Windows (mandato del programma per Windows) sono da sistemare e provare gli argomenti con lettere accentate e gli a
  capo CR LF, e da provare la lettura blindata (pipe, dispositivi, collegamenti e scorciatoie, cartelle sincronizzate come
  OneDrive con i "file su richiesta") e le protezioni del compilatore equivalenti (/GS, /guard:cf, /DYNAMICBASE,
  /HIGHENTROPYVA, /NXCOMPAT): oggi sono provate solo su Linux.
- Sicurezza (docs/sicurezza.md): restano i nomi liberi del demo (il turno "Centro diurno", il servizio "Trasporto al centro
  diurno Il Faro", che nel JSON della giornata nomina il centro): li tocca il mandato "avviso sui nomi liberi". Chiusi dal
  K-010: dati in chiaro sul disco (ora nella cassaforte), un solo utente, nessun registro degli accessi, il centro letto
  accanto al codice della casa con un'altra uscita (nome e casa di chi va al centro sono dati particolari in tutte le
  uscite). Un file vicino ai 256 MB preparato apposta puo' occupare diversi GB di memoria
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
- Il fuso orario del manifesto versione 1 non ha una lunghezza massima nello schema (la versione 2 del K-010 si': 64
  caratteri): resta l'unica eccezione alla prova dei limiti di buon senso, finche' si leggono manifesti della versione 1.
- Cassaforte (K-010, docs/sicurezza.md e docs/cassaforte.md): una copia vecchia della cartella rimessa al posto di quella
  nuova (es. da un utente tolto) non si scopre senza un riferimento fuori dalla cartella, e si apre con le password di
  allora; le copie vecchie e quelle di sicurezza restano cifrate con le chiavi di allora anche dopo la cancellazione. Chi
  ha le chiavi e sa programmare puo' riscrivere il registro; l'ora del registro e' quella dell'orologio del PC. Le chiavi
  e i testi decifrati stanno in memoria protetta, ma i dati uniti restano nelle strutture normali del programma fino alla
  fine del comando. Il blocco della cartella (flock) su una cartella di rete dipende dal server.
- Windows (programma per Windows): per la cassaforte sono da fare e da provare li' la password dal terminale senza eco, il
  blocco della cartella, la sostituzione atomica dei file, la memoria protetta e il divieto dei "core dump"; oggi la
  costruzione di libsodium si ferma con un messaggio chiaro su un sistema non POSIX.
- Orari di lavoro (K-011), casi che il programma non tratta: piu' conducenti a bordo (multipresenza, Reg. 561 art. 4 o,
  7 e 8 c. 5: avviso K-CTR-108 se compare); le deroghe dei servizi occasionali lunghi (art. 8 c. 2 bis e 6 bis); l'art.
  9 (traghetti e treni, tempo per raggiungere il mezzo) e l'art. 12 (deroghe del conducente) del 561. Per i dipendenti a
  cui non vale la L. 138 (es. servizi non di linea fuori dal 561) la legge sugli orari da applicare e' da verificare:
  oggi un avviso (K-CTR-107) e quegli orari non si giudicano. Il contratto collettivo vero (es. IC35) arrivera' dal
  consulente del lavoro, come regime nei dati. Il registro di servizio (art. 16 del Reg. 561, per le linee fino a 50 km)
  arrivera' con le stampe. Le letture "da confermare" sono in REGOLE.md, da portare al consulente.
- (K-013, chiuso dal K-014) Senza i dati particolari K-CTR-068 (autorizzazione del committente) e K-CTR-069 (uso del mezzo)
  dicevano il tipo di servizio: ora si contano soltanto, con K-CTR-072 (vincolo per tipo) e K-CTR-067 varianti 4 e 6.
- (K-014) Senza i dati particolari si vede che un turno e' scoperto o senza mezzo, ma non il perche' (scelta A del supervisore): chi
  conosce bene assenze, fermi e scadenze (es. la distrazione di ALFA-05 che scade il 2027-06-30, K-CTR-050, sempre visibile)
  puo' a volte intuire che il motivo e' un requisito del tipo di servizio (docs/sicurezza.md). Il testo senza il motivo vale per
  ogni causa appena il pacchetto ha un requisito che dipende dal tipo, anche un vincolo di destinazione che non toglie mai un
  mezzo: una regola piu' stretta (solo autorizzazione del committente e usi) nasconderebbe meno, da decidere se servira'.
- (K-014) Le condizioni dei regolamenti con tipi di servizio (assistente a bordo, adulto alla fermata) sono riconosciute tra i
  requisiti del tipo, ma oggi nessuna uscita le usa: la verifica delle regole dovra' contare soltanto i loro problemi senza i
  dati particolari.
- (K-013) Senza i dati particolari i problemi degli orari dei regimi che dipendono dal tipo di servizio si contano
  soltanto: chi pianifica e non chiede --includi-dati-particolari non vede, per esempio, le violazioni del Reg. 561 (come
  per tutti i dati sulla salute dal K-009); kzero ore lo dice in una riga fissa per persona.
- kzero ore e kzero controlla rifanno tutti i controlli a ogni richiesta. Con la prova di scala mista del K-012 (1.320
  persone, un anno, 9.800 corse): kzero controlla circa 13 secondi; kzero ore di una persona per l'anno 13 secondi, di tutti
  per una settimana 14, per un mese 17; di tutti per l'anno 44 secondi in testo (243 MB di uscita, 1,9 GB di memoria) e circa
  71 secondi in JSON (1,8 GB di uscita, 3,8 GB di memoria, perche' l'uscita si prepara tutta prima di scriverla). Per periodi
  lunghi di tutte le persone conviene chiedere un mese alla volta.
- (K-012) Un contratto vuole sempre un regolamento, anche per il noleggio di un privato: nel demo i contratti dei privati
  usano il regolamento inventato R-INV. Se servira', un contratto senza regolamento (o un regolamento "nessuno") per l'NCC.
- (K-012) Nella prova di scala linee, NCC e disabili usano il calendario dei giorni di scuola, come lo scolastico: una
  semplificazione per avere turni regolari; i weekend e le linee festive sono provati nel demo.
- (K-012) Il nome di un ente committente si vede sempre (dato comune); il nome di un committente privato non deve stare nei
  nomi dei servizi e dei contratti: oggi lo dice solo il formato, l'avviso arriva con il mandato dei nomi liberi.

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
- (supervisore, 13/10) Ricerca del mezzo adatto per un servizio nuovo o ipotetico: dati orario ed esigenze (posti,
  carrozzine, ausili piegati, assistente), proporre i mezzi in ordine di convenienza (capienza, pedana,
  bagagliaio, dove si trova il mezzo a quell'ora, km a vuoto, regole sull'orario di lavoro degli autisti, es. oltre
  9 posti su un servizio di oltre 50 km cambiano le regole della settimana). Possibile dopo i turni (K-008), le
  distanze e la verifica delle regole.
- Distribuzione dei lavori extra (gite, noleggi) a rotazione o per anzianita', secondo la scelta dell'azienda.
- Scelta dei turni da parte degli autisti (preferenze espresse, poi il programma propone e l'azienda decide).
- Servizi a chiamata (prenotati il giorno prima o in giornata), da inserire nei turni gia' fatti.
- (K-010) Chiave di ripristino della cassaforte (oggi non c'e': persa la password, un altro utente con gli stessi permessi
  ne da' una nuova, altrimenti i dati si riprendono dal file del consorzio). Se servira': una chiave stampata su carta, data a
  chi decide l'azienda, con le stesse regole di una password.
- (K-010) Accorgersi di una copia vecchia della cassaforte rimessa al suo posto: il programma ricorda, fuori dalla
  cartella (es. nella cartella dell'utente sul PC), per ogni cassaforte il numero della chiusura e la versione delle
  chiavi piu' alti visti, e avvisa se ne trova di piu' bassi.
- (K-010 aggiunta 1) Password scritta da un altro (utente aggiunto, password persa): oggi lo dice solo l'uscita del
  comando a chi l'ha scritta (K-CTR-100). Per ricordarlo anche al nuovo utente a ogni sua apertura, finche' non la
  cambia, serve un segno nella sua fessura ("password scritta da un altro"): un dato in piu' nel formato.

## Prossimi passi
1. Mandato piccolo "avviso sui nomi liberi": un avviso quando un nome libero (servizi, gruppi, turni, scuole e centri)
   lascia capire un dato personale o particolare.
2. Lettore del file alunni del consorzio, che lo trasforma nel formato K-Zero (con la lettura blindata del K-009) e scrive i
   dati personali e particolari solo nella cassaforte, mai in file in chiaro.
3. Scadenziario: le scadenze in un posto solo (abilitazioni, autorizzazioni, revisioni, contratti, data di
   conservazione, giornate a chiamata vicine al limite).
4. Viste per zona e per ora: giornata e orari di un gruppo di comuni o di una fascia oraria.
5. Programma per Windows da scaricare (i dati veri restano sul PC del supervisore), con le prove di sicurezza su Windows (anche
   quelle della cassaforte) e la correzione dei dati senza file in chiaro.
6. Raccolta dati veri ALP: mezzi, autisti (paese), giri di una giornata tipo; le letture "da confermare" degli
   orari di lavoro al consulente del lavoro (REGOLE.md).
7. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
8. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
