# K-Zero

K-Zero e' universale: aziende, consorzi ed enti di ogni dimensione e di ogni paese; Alpinabus e' il primo caso di prova.

Software di pianificazione flotta: servizi di linea, handicap, scolastici, ibridi.
Si parte da Alpinabus (3 minibus + 1 auto) e si deve poter crescere fino a 500 mezzi e oltre.

## Confini
- IMPORTANTE: questo repository riguarda SOLO K-Zero. Sullo stesso account GitHub esiste un
  progetto musicale separato: non leggerlo, non modificarlo, non citarlo, non mescolare file.

## Ruoli
- Mauro: supervisore. Decide le regole del mestiere e approva i risultati.
- Claude Code: programmatore, unico che scrive nel repository GitHub.
- Claude in chat (progetto K-Zero): arbitro. Rivede piani e modifiche, non scrive codice nel repository.

## Come si salvano le modifiche
- Solo documenti (DIARIO.md, REGOLE.md, DECISIONI.md, altre note): salva direttamente
  su main e annota nel diario. L'arbitro controlla dopo.
- Programma (codice, CMakeLists.txt, test) e CLAUDE.md: ramo di lavoro, poi proposta
  (pull request), poi controllo dell'arbitro, poi Mauro approva con Merge.
  Non unire mai una proposta da solo.

## Comandi
- Compilare: `cmake -S . -B build && cmake --build build`
- Descrizione completa del programma (comandi, parametri, schemi, errori): `./build/kzero descrivi`
  (per i programmi e le AI: `./build/kzero descrivi --formato json`)
- Versione: `./build/kzero versione`
- Controllo di un pacchetto dati (formato in docs/formato-dati.md, problemi K-CTR in docs/controlli.md):
  `./build/kzero controlla <cartella>` (es. `./build/kzero controlla esempi/demo`; `--formato json`;
  uscita 1 se ci sono errori nei dati, gli avvisi non bloccano)
- Stampa di prova con il pacchetto dimostrativo esempi/demo (dati inventati): `./build/kzero dimostrazione`
  (`--formato json`; `--includi-dati-personali` e `--includi-dati-particolari` per vedere i dati nascosti)
- Eseguire le prove: `ctest --test-dir build --output-on-failure`
- Le prove sugli schemi JSON (`schema.*`) richiedono lo strumento standard check-jsonschema (versioni e
  impronte fissate), da installare prima di configurare in un ambiente Python dedicato:
  `python3 -m venv ~/.kzero-strumenti && ~/.kzero-strumenti/bin/pip install --require-hashes -r tests/strumenti/check-jsonschema.txt`
  e poi `export PATH=~/.kzero-strumenti/bin:$PATH`. Senza lo strumento quelle prove non vengono registrate
  (CMake lo segnala); il controllo GitHub le rende obbligatorie (`-DKZERO_SCHEMI_OBBLIGATORI=ON`).

## Regole di lavoro
- IMPORTANTE: K-Zero è un software professionale: stabile, solido, preciso.
  Niente pezze né scorciatoie: solo soluzioni che durano nel tempo.
  Se una soluzione rapida e una solida divergono, scegli la solida e spiega perché.
- IMPORTANTE: Mauro non e' un tecnico. Spiegazioni in italiano semplice, brevi, senza gergo.
- Prima di dire "fatto": compilare senza warning, eseguire, mostrare l'output.
- Ogni modifica va annotata in DIARIO.md. Le scelte di fondo vanno in DECISIONI.md.
- Le regole del mestiere sono in REGOLE.md: leggerle prima di toccare il calcolo.
- Ogni lavoro nasce da un mandato con codice K-xxx (es. K-005).
  Il codice va nel titolo della proposta e nella riga del DIARIO.

## Scelte tecniche
- C++17, CMake 3.25 o successivo, Google OR-Tools (non ancora collegato).
- Orari in minuti dalla mezzanotte, distanze in km.
- Fonte delle distanze intercambiabile: Google Maps per Alpinabus, OSRM per la scala provinciale.
- Un solo modello dati per 4 o 500 mezzi; cambia solo il metodo di calcolo.
- Tre piani (docs/architettura.md): nucleo in src/nucleo (modello, regole, calcolo: niente stampa, niente
  riga di comando); API in src/api (il contratto: riga di comando con uscita testo/JSON); adattatori sopra l'API.
- Librerie con versione e impronta fissate: CLI11 2.7.2 (parametri), nlohmann/json 3.12.0 (JSON),
  GoogleTest 1.15.2 (prove), valijson 1.1.3 (controllo dei dati con gli schemi; fissata con l'impronta del commit).
- Dati: pacchetto = cartella con manifesto e un file JSON per tipo di dato (docs/formato-dati.md); schemi in
  docs/schema, incorporati nel programma insieme al pacchetto esempi/demo.
- Nessun valore di regola e nessun nome di committente nel codice: le regole di un territorio o di un
  committente stanno nei regolamenti (dati), le parole di un'azienda sono solo nomi nei dati.

### Principi uomo + AI (valgono per ogni comando)
- Un solo eseguibile `kzero` con comandi: `kzero <comando> [parametri]`, il comando sempre per primo.
  Nessuna funzione raggiungibile solo da un menu.
- Comandi e parametri si dichiarano una volta sola, nella tabella di src/api/comandi.cpp:
  da li' nascono la lettura dei parametri e `kzero descrivi`.
- `--formato testo` (italiano chiaro, predefinito) oppure `--formato json`.
- Ogni uscita JSON contiene "schema" e "schema_versione". Schemi in docs/schema/ (JSON Schema draft-07),
  file `docs/schema/<schema>.v<schema_versione>.schema.json`. Se la forma di un'uscita cambia:
  nuova schema_versione e nuovo file di schema.
- Errori: codice fisso K-ERR-nnn, messaggio in italiano, posizione (file, riga, colonna) quando esiste.
  In modalita' json anche l'errore e' JSON. Elenco completo in docs/errori.md (una prova lo confronta
  con il programma). Un nuovo errore prende il codice successivo; un codice non si riusa mai.
- Il risultato va sull'uscita standard, gli errori sull'uscita di errore; con un errore il risultato resta vuoto.
- Codici di uscita: 0 riuscito, 1 errore nei dati, 2 errore nei parametri, 3 errore interno.
- Stesso input = stesso output: ordinamenti stabili, nessun uso dell'orologio o del caso.
  Ogni comando che dipende da una data la riceve come parametro (`--data AAAA-MM-GG`).
- Di base nessun comando scrive file; per scrivere serve `--scrivi <percorso>`.
- Tre classi di dati, marcate campo per campo negli schemi ("x-kzero-classe"): comuni, personali (nomi,
  indirizzi, case, punti porta a porta, motivi scritti a mano), particolari (salute: esigenze, carrozzine,
  assistente necessario, servizio per disabili). Nelle uscite, testo e JSON allo stesso modo: di base i
  personali sono sostituiti dal codice e i particolari diventano "riservato"; si vedono solo con
  `--includi-dati-personali` e `--includi-dati-particolari`. Vale anche nei messaggi.
- Due livelli di pacchetto: "solo numeri" (consigliato: conteggi, niente nomi, case, porta a porta, motivi)
  e "completo". Mai codice fiscale, telefoni, mail: il formato non ha campi per questi dati.
- Problemi nei dati: codice fisso K-CTR-nnn (catalogo separato dai K-ERR, in docs/controlli.md, provato
  contro il programma), livello errore o avviso, posizione = file + percorso JSON. Testi in un unico catalogo.

## Trappole
- "Ibrido" non si inserisce a mano: lo ricava derive_service.
- L'assistente occupa un posto. Solo chi viaggia seduto sulla carrozzina toglie posti a sedere e chiede la
  pedana; la carrozzina piegata e gli altri ausili piegati vanno nel bagagliaio, oppure tra i sedili (occupando
  posti liberi) solo se il mezzo lo ammette. Tutto in fits(), una sola regola.
- Pause a casa dell'autista e ricoveri esterni sono preferenze, mai obblighi.
- I dati di esempi/demo, esempi/demo-numeri e della prova di scala sono inventati (tranne il regolamento
  R-TN, che riporta regole pubbliche con la fonte): non usarli come dati reali.
- Un orario oltre le 24:00 resta tale ("24:12"), anche in uscita: non riportarlo alla mezzanotte.
- Posti dei mezzi = posti per i passeggeri, conducente escluso (sul libretto il campo S.1 lo comprende).
