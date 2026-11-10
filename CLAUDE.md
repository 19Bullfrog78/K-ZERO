# K-Zero

K-Zero e' universale: aziende, consorzi ed enti di ogni dimensione e di ogni paese; ALP e' il primo caso di prova.

Software di pianificazione flotta: servizi di linea, handicap, scolastici, NCC, promiscui.
Si parte da ALP (3 minibus + 1 auto) e si deve poter crescere fino a 500 mezzi e oltre.

## Confini
- IMPORTANTE: questo repository riguarda SOLO K-Zero. Sullo stesso account GitHub esiste un
  progetto musicale separato: non leggerlo, non modificarlo, non citarlo, non mescolare file.
- IMPORTANTE (supervisore, 5/11 e 8/11), deposito pubblico: il deposito e' pubblico dal 5/11/2026. Nessun dato vero che
  non sia gia' pubblico (di ALP o di altri: persone, passeggeri, mezzi e targhe, codici interni, orari interni, file del
  consorzio) entra nel deposito, nemmeno cifrato in una cassaforte: i dati veri restano solo sul PC del supervisore, fuori
  dalla cartella del deposito. I dati pubblici vanno bene (luoghi, linee e fermate del trasporto pubblico, leggi e
  regolamenti con la loro fonte). Pacchetti e casi di prova solo con dati inventati (piu' i dati pubblici, come R-TN).
  Nei testi nuovi, nei messaggi dei commit, nei titoli delle proposte, nei resoconti e nei messaggi all'arbitro mai i
  quattro nomi che il K-015 ha tolto (nell'ordine a-d dei messaggi del controllo): si scrivono "ALP", "il supervisore",
  "il consorzio" (nei codici esterni il sistema "Consorzio"), "l'azienda del trasporto pubblico provinciale" (nei codici
  esterni il sistema "Trasporti Provinciali"). La storia prima del 5/11 resta com'e'. Reti, che non sostituiscono la
  regola (docs/sicurezza.md): .gitignore, la prova `deposito` e il controllo GitHub di titolo, descrizione e commit delle
  proposte.
- IMPORTANTE (K-016): nel deposito i testi sono UTF-8; i binari solo dall'elenco dei binari ammessi (oggi i file .kzc e
  .kzr dentro tests/ ed esempi/; l'elenco sta in tests/strumenti/deposito.cpp), che si allarga solo con un mandato. Un
  testo scritto in un'altra codifica (es. UTF-16 da un programma di Windows) si salva di nuovo in UTF-8.

## Ruoli
- Il supervisore: decide le regole del mestiere e approva i risultati.
- Claude Code: programmatore, unico che scrive nel repository GitHub.
- Claude in chat (progetto K-Zero): arbitro. Rivede piani e modifiche, non scrive codice nel repository.

## Come si salvano le modifiche
- IMPORTANTE (K-016): niente salvataggi diretti su main. Ogni modifica, anche di soli documenti (DIARIO.md, REGOLE.md,
  DECISIONI.md, altre note), va su un ramo di lavoro e in una proposta (pull request), e si annota nel diario.
- Programma (codice, CMakeLists.txt, prove) e CLAUDE.md: proposta, poi controllo dell'arbitro, poi il supervisore approva
  con Merge.
- Solo documenti: la proposta ha un titolo che lo dice (es. "Documenti: ...") o il codice del mandato; la unisce il
  supervisore quando i quattro controlli sono verdi (compila-e-prova, sanitizer e fuzzing del "Controllo",
  titolo-descrizione-e-commit dei "Nomi della proposta"); l'arbitro la guarda quando il supervisore lo chiede.
- Non unire mai una proposta da solo.
- Perche': il supervisore rende obbligatori su main la proposta e i controlli verdi, quindi un salvataggio diretto verrebbe
  rifiutato, e cosi' anche i messaggi dei documenti passano dal controllo dei nomi.
- IMPORTANTE (supervisore, 5/11): ogni resoconto per l'arbitro (es. K-013-resoconto-per-l-arbitro.md) e ogni documento utile al
  progetto si consegna al supervisore sempre come file da scaricare (non solo come testo nel messaggio), con il suo nome: il
  supervisore lo mette nella cartella giusta del suo PC (i resoconti in `C:\Users\BULLFROG\Desktop\LAVORO\K-ZERO\K-ZERO CC`).
  Una sessione nel cloud non raggiunge il PC del supervisore: mai dire "salvato sul PC" se non lo e'. Solo se la sessione gira
  sul PC del supervisore (o la cartella e' collegata) il file si scrive anche direttamente li'.

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
- Giornata di un giorno (turni, chi li fa, mezzi, corse e attivita', turni scoperti o senza mezzo):
  `./build/kzero giornata <cartella> --data AAAA-MM-GG [--persona CODICE | --mezzo CODICE]`
  (es. `./build/kzero giornata esempi/demo --data 2026-10-22 --persona P-C`; `--formato json`; con errori nei dati
  non mostra nulla e rimanda a kzero controlla, errore K-ERR-018; gli errori del piano degli orari non bloccano)
- Orari di lavoro (K-011) di un giorno o di un periodo (tempi per categoria, lavoro per regime, riposi, leggi che valgono e
  perche', rientri, problemi): `./build/kzero ore <cartella> (--data AAAA-MM-GG | --dal AAAA-MM-GG --al AAAA-MM-GG)
  [--persona CODICE]` (es. `./build/kzero ore esempi/demo --dal 2026-11-09 --al 2026-11-15 --persona P-A`; `--formato
  json`; pacchetto di prova degli orari: tests/esempi/k011-orari, che resta alle versioni 2 del K-011; dal K-012 pacchetto
  di prova dei servizi, rifatto sul demo di oggi: tests/esempi/k012-servizi, con tests/strumenti/genera_k012.py)
- Cassaforte dei dati personali (K-010, docs/cassaforte.md): `./build/kzero chiudi <cartella in chiaro> --scrivi <cartella
  nuova> --utente CODICE --conservazione AAAA-MM-GG`; poi ogni comando su un pacchetto chiuso vuole `--utente CODICE` e la
  password (dal terminale senza eco, oppure dall'ingresso standard con `--password-da-ingresso`; mai come parametro ne' in
  una variabile d'ambiente). Altri comandi: `cassaforte`, `registro`, `esporta` e `richiudi` (correggere i dati),
  `aggiungi-utente`, `togli-utente`, `password`, `conservazione`, `cancella`; quelli che cambiano la cassaforte vogliono
  `--scrivi <cartella del pacchetto>`. Copia congelata della versione 1 con le password di prova: tests/esempi/k010-cassaforte.
- Eseguire le prove: `ctest --test-dir build --output-on-failure`
- Controllo del deposito pubblico (K-015: i quattro nomi in nessun file, nessun file di dati ne' manifesto di un pacchetto
  fuori da tests/ ed esempi/; K-016: solo testi UTF-8 e binari ammessi, cartelle saltate uguali a quelle di .gitignore):
  `ctest --test-dir build -L deposito --output-on-failure`; il messaggio dice file, riga e quale nome (a-d), mai il nome.
  Prima di aprire una proposta anche il titolo, la descrizione (scritta in un file fuori dalla cartella del deposito, es.
  ../descrizione.md) e i messaggi dei commit nuovi (lo stesso controllo che fa GitHub): `TITOLO="K-xxx: ..."
  DESCRIZIONE="$(cat ../descrizione.md)" tests/strumenti/controlla_proposta.sh build/tests/controlla_deposito origin/main
  HEAD` (senza descrizione `DESCRIZIONE=""`).
- Le prove sugli schemi JSON (`schema.*`) richiedono lo strumento standard check-jsonschema (versioni e
  impronte fissate), da installare prima di configurare in un ambiente Python dedicato:
  `python3 -m venv ~/.kzero-strumenti && ~/.kzero-strumenti/bin/pip install --require-hashes -r tests/strumenti/check-jsonschema.txt`
  e poi `export PATH=~/.kzero-strumenti/bin:$PATH`. Senza lo strumento quelle prove non vengono registrate
  (CMake lo segnala); il controllo GitHub le rende obbligatorie (`-DKZERO_SCHEMI_OBBLIGATORI=ON`).
- Prove con i sanitizer (AddressSanitizer e UndefinedBehaviorSanitizer, si fermano al primo problema; clang come nel
  controllo GitHub; su Ubuntu serve il pacchetto libclang-rt-18-dev): `CC=clang CXX=clang++ cmake -S . -B build-san -DKZERO_SANITIZER=ON -DKZERO_WERROR=ON
  -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build-san && ctest --test-dir build-san --output-on-failure`
- Fuzzing (prove con dati casuali e deformati, libFuzzer, solo clang; su Ubuntu serve il pacchetto libclang-rt-18-dev):
  `CC=clang CXX=clang++ cmake -S . -B build-fuzz -DKZERO_FUZZ=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo &&
  cmake --build build-fuzz --target fuzz_pacchetto fuzz_parametri fuzz_date_e_orari fuzz_cassaforte rigioca_fuzz`
  (rigioca_fuzz prepara i semi della cassaforte, con chiavi nuove a ogni giro), poi
  `tests/fuzz/fuzz.sh build-fuzz <secondi per bersaglio> [cartella di lavoro] [bersagli]` (32 bersagli: `pacchetto-<file>`,
  `parametri`, `date-e-orari` e, dal K-010, `cassaforte`, `cassaforte-voci`, `registro`, `registro-voci`, `password`; nel
  controllo GitHub 20 secondi; in locale a lungo, es. `KZERO_FUZZ_PARALLELI=4
  tests/fuzz/fuzz.sh build-fuzz 3600 ~/kzero-fuzz`). Un guasto lascia il file che lo provoca in
  `<lavoro>/guasti/<bersaglio>/`: si corregge e il file va in `tests/fuzz/casi/<bersaglio>/` (diventa una prova normale).
- Sicurezza (cosa protegge K-Zero, rischi, consigli, violazioni): docs/sicurezza.md; elenco dei componenti esterni:
  docs/componenti.cdx.json (CycloneDX), da aggiornare a ogni libreria nuova o cambiata (una prova lo controlla).

## Principi guida
- IMPORTANTE (supervisore, 13/10): K-Zero non si calibra su un'azienda ed e' personalizzabile dall'azienda il piu'
  possibile. L'organizzazione (rotazioni, giorni, chi fa cosa, nomi, tipi di attivita', assenze, fermi,
  competenze, impostazioni) la scrive e la cambia l'utente nei dati; il programma la registra, la controlla e
  piu' avanti la proporra', ma non decide niente da solo e ogni scelta dell'utente vince. ALP e' solo un
  esempio nei dati.
- IMPORTANTE (supervisore, risposta 43), domande e AI: le domande all'utente le fara' il programma stesso, a scelta multipla,
  senza AI, con le risposte salvate nei dati. Un assistente AI e' solo un aiuto facoltativo e di base non riceve ne'
  nomi ne' dati sulla salute (le uscite di base sono gia' filtrate).
- IMPORTANTE (K-011): le regole sugli orari sono dati (legge, contratto collettivo, azienda, persona); il programma conosce
  solo i tipi di regola; la legge certa e' un errore, quella da confermare un avviso; senza dati mai "regolare".
- IMPORTANTE (supervisore, 18/10), sicurezza: il programma lavora sul PC e oggi nessun comando usa la rete; quando
  servira' (es. le distanze da Google Maps) lo fara' solo un adattatore separato, acceso con un parametro
  esplicito, che manda fuori il minimo (coordinate, mai nomi o altri dati delle persone), con un suo mandato.
  Non scrive file senza --scrivi; controlla ogni dato in ingresso con limiti di buon senso e rifiuta i file
  malformati senza bloccarsi; conserva il minimo dei dati personali. I dati veri entrano solo dopo la sicurezza di base
  (K-009) e la cassaforte dei dati personali (K-010: cifratura, utenti e permessi, registro degli accessi). Dettagli e
  rischi che restano: docs/sicurezza.md e docs/cassaforte.md.

## Regole di lavoro
- IMPORTANTE: K-Zero è un software professionale: stabile, solido, preciso.
  Niente pezze né scorciatoie: solo soluzioni che durano nel tempo.
  Se una soluzione rapida e una solida divergono, scegli la solida e spiega perché.
- IMPORTANTE: Il supervisore non e' un tecnico. Spiegazioni in italiano semplice, brevi, senza gergo.
- Prima di dire "fatto": compilare senza warning, eseguire, mostrare l'output.
- Ogni modifica va annotata in DIARIO.md. Le scelte di fondo vanno in DECISIONI.md.
- Le regole del mestiere sono in REGOLE.md: leggerle prima di toccare il calcolo.
- Ogni lavoro nasce da un mandato con codice K-xxx (es. K-005).
  Il codice va nel titolo della proposta e nella riga del DIARIO.

## Scelte tecniche
- C++17, CMake 3.25 o successivo, Google OR-Tools (non ancora collegato).
- Orari in minuti dalla mezzanotte, distanze in km.
- Fonte delle distanze intercambiabile: Google Maps per ALP, OSRM per la scala provinciale.
- Un solo modello dati per 4 o 500 mezzi; cambia solo il metodo di calcolo.
- Tre piani (docs/architettura.md): nucleo in src/nucleo (modello, regole, calcolo: niente stampa, niente
  riga di comando); API in src/api (il contratto: riga di comando con uscita testo/JSON); adattatori sopra l'API.
- Librerie con versione e impronta fissate: CLI11 2.7.2 (parametri), nlohmann/json 3.12.0 (JSON),
  GoogleTest 1.15.2 (prove), valijson 1.1.3 (controllo dei dati con gli schemi; fissata con l'impronta del commit);
  dal K-010 libsodium 1.0.22 (cifratura, solo le sue primitive come le documenta), utf8proc 2.11.3 (normalizzazione delle
  password) e l'elenco delle password comuni di SecLists (fissato con il commit e l'impronta del file).
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
- Eccezioni della cassaforte (K-010), le uniche: l'orologio (ora delle voci del registro, scadenza della data di
  conservazione), il caso (chiavi, sali, nonce: le uscite restano uguali, i byte della cassaforte no) e le scritture del
  registro degli accessi senza `--scrivi`. Nessun parametro cambia l'ora; un'ora fissa e Argon2id leggero solo nelle prove
  interne (accesso::Ambiente), mai nel programma kzero. I comandi che cambiano la cassaforte vogliono `--scrivi`.
- Tre classi di dati, marcate campo per campo negli schemi ("x-kzero-classe"): comuni, personali (nomi,
  indirizzi, case, punti porta a porta, motivi scritti a mano), particolari (salute: esigenze, carrozzine,
  assistente necessario, servizio per disabili); dal K-012 nome e nota di un committente privato sono personali (legati
  se un suo servizio va in un luogo sensibile, regola in LegameSensibile). Nelle uscite, testo e JSON allo stesso modo: di base i
  personali sono sostituiti dal codice e i particolari diventano "riservato"; si vedono solo con
  `--includi-dati-personali` e `--includi-dati-particolari`, e con un pacchetto chiuso solo entro i permessi dell'utente
  (oltre i permessi e' un errore K-ERR, mai un'uscita filtrata in silenzio). Vale anche nei messaggi, nelle posizioni (mai dentro un
  dato personale o particolare: si fermano all'elemento comune piu' vicino, regola ricavata dagli schemi) e nei
  problemi stessi (ogni controllo K-CTR dichiara cosa rivela; quelli sui dati sulla salute di base si contano
  soltanto). Con i nomi dei passeggeri o le loro case e senza i dati particolari, il legame con un luogo sensibile e'
  riservato in tutto l'insieme che l'uscita lega (il messaggio, servizi e corse, l'intera giornata): una regola sola,
  `LegameSensibile` in src/api/riservatezza.hpp. Dal K-010 segue i permessi, non i parametri, e nome e casa di chi va in
  un luogo sensibile sono dati particolari in tutte le uscite (DECISIONI.md). Dal K-013, senza i dati particolari (permesso o
  parametro) negli orari e' riservato tutto quello che dipende dal tipo di servizio delle corse: i regimi che ne dipendono
  (lo dicono i dati, `kind_dependence` nel nucleo), il lavoro che contano, il contratto e i loro problemi (contati soltanto,
  `Problem::by_service_kind` e `dal_tipo` nel catalogo), con una riga fissa per persona; ogni nuova uscita degli orari passa
  da li' e la prova per differenza (tests/test_unione.cpp) lo controlla. Dal K-014 vale anche per i requisiti che dipendono
  dal tipo (usi dei mezzi, autorizzazione del committente, vincolo di destinazione, condizioni dei regolamenti:
  `kind_requirements` nel nucleo): i loro problemi si contano soltanto e, se il pacchetto ne ha uno, di ogni turno scoperto o
  senza mezzo si vede il fatto ma non il perche', con lo stesso testo per ogni causa (scelta A del supervisore,
  `motivo_dei_turni_riservato` in src/api/riservatezza.hpp); ogni nuovo requisito o uscita sul piano passa da li'.
- Cassaforte (K-010): i dati personali e particolari veri stanno solo nella cassaforte del pacchetto; ogni nuovo lettore
  (es. il file del consorzio) li scrive solo li', mai in file in chiaro. Dati decifrati mai su disco ne' in file temporanei
  (l'unica eccezione e' l'uscita in chiaro esplicita di `kzero esporta --scrivi`). Ogni nuova uscita prende la visibilita'
  dai permessi e dai parametri insieme; un controllo o un calcolo che ha bisogno di una classe che l'utente non puo'
  aprire dice "non verificato", mai un risultato fatto su dati mancanti. Ogni apertura scrive prima la voce del registro:
  senza voce, nessun dato.
- Ogni nuova uscita, messaggio, posizione o registro passa dal filtro delle classi; nessun dato personale nei registri
  del programma. La lettura dei file e' blindata (solo file normali dentro la cartella del pacchetto, mai oltre il
  massimo di byte): ogni nuovo lettore di file esterni (es. il file del consorzio) la usa.
- Due livelli di pacchetto: "solo numeri" (consigliato: conteggi, niente nomi, case, porta a porta, motivi)
  e "completo". Mai codice fiscale, telefoni, mail: il formato non ha campi per questi dati.
- Problemi nei dati: codice fisso K-CTR-nnn (catalogo separato dai K-ERR, in docs/controlli.md, provato
  contro il programma), livello errore o avviso, posizione = file + percorso JSON. Testi in un unico catalogo.

## Trappole
- "Promiscuo" (fino al K-011 "ibrido") non si inserisce a mano: lo ricava derive_service. Per un servizio NCC i gruppi non
  cambiano la categoria (una gita scolastica e' NCC); il tipo NCC dice la natura del servizio, non la licenza del mezzo
  (una navetta di operai con tragitto e orari fissi non e' NCC). L'alimentazione "ibrido" dei mezzi e' un'altra cosa.
- L'assistente occupa un posto. Solo chi viaggia seduto sulla carrozzina toglie posti a sedere e chiede la
  pedana; la carrozzina piegata e gli altri ausili piegati vanno nel bagagliaio, oppure tra i sedili (occupando
  posti liberi) solo se il mezzo lo ammette. Tutto in fits(), una sola regola.
- La chiave del rientro e' nei dati dell'azienda e della persona; senza distanze il programma giudica solo quando e' certo.
- Chi fa un turno: assegnazione del singolo giorno, poi servizio fisso, poi rotazione (vince il piu' preciso).
  Se nessuno lo da', il turno e' scoperto: il programma non inventa assegnazioni. Un fatto con le date
  (assenza, scadenza, fermo) su un turno ricorrente e' un avviso; sull'assegnazione del singolo giorno e' un errore.
- Un turno ha un solo mezzo: il cambio di mezzo e il cambio dell'autista sono turni diversi.
- I dati di esempi/demo, esempi/demo-numeri e della prova di scala sono inventati (tranne il regolamento
  R-TN, che riporta regole pubbliche con la fonte): non usarli come dati reali.
- Un orario oltre le 24:00 resta tale ("24:12"), anche in uscita: non riportarlo alla mezzanotte.
- Posti dei mezzi = posti per i passeggeri, conducente escluso (sul libretto il campo S.1 lo comprende).
- Cassaforte (K-010): un valore si lega al suo elemento per codice (o per la chiave dichiarata nello schema,
  `x-kzero-chiave`), mai per posizione; un valore senza il suo elemento e' un errore K-CTR, mai un'unione sbagliata.
  Chi ha solo i dati personali non riceve mai la chiave dei particolari, nemmeno durante il cambio delle chiavi.
- Cassaforte (K-010 aggiunta 1): i dati personali di chi va in un luogo sensibile stanno nella sezione dei legati, con
  una chiave ricavata da quelle dei personali e dei particolari insieme: si ricifra a ogni cambio di una delle due, e
  togliere un utente vuole tutti e due i permessi. Gli avvisi della cassaforte (password da cambiare, conservazione) hanno
  i testi nel catalogo dei controlli (K-CTR-097 ... 100), mai scritti a parte in un comando.
