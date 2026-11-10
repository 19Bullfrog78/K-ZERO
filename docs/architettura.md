# Architettura di K-Zero

## In breve

K-Zero e' universale: aziende, consorzi ed enti di ogni dimensione e di ogni paese; nel programma non c'e'
nulla di specifico di un'azienda o di un territorio (le regole locali sono dati).

K-Zero ha due utenti: **una persona** (chi pianifica) e **un'AI** (un assistente che lavora per lei).
Tutti e due usano lo stesso programma, `kzero`, con gli stessi comandi. La persona legge l'uscita
in **testo** (italiano chiaro); l'AI legge l'uscita in **JSON**, descritta da **schemi** pubblici.
Gli errori del programma hanno sempre un **codice fisso** (`K-ERR-nnn`), i problemi nei dati anche
(`K-CTR-nnn`, vedi docs/controlli.md).

Il programma e' diviso in tre piani, uno sopra l'altro:

```
   persona                          assistente AI
      |                                   |
 [ finestra per persone ]        [ adattatore MCP ]        piano 3: ADATTATORI (nessuna logica)
      |                                   |
      +-----------------+-----------------+
                        |                                  (un agente AI puo' gia' usare
             [ kzero <comando> ]                            direttamente la riga di comando + JSON)
                        |
   piano 2: API = il contratto ufficiale (riga di comando, testo/JSON, schemi, errori)
                        |
   piano 1: NUCLEO = modello dati, regole del mestiere, calcolo
```

## Piano 1 - Nucleo (`src/nucleo`)

- Contiene il modello dati, le regole del mestiere (REGOLE.md) e il calcolo (OR-Tools, quando arrivera').
- **Non stampa nulla, non scrive file, non legge la riga di comando** e non usa le librerie dell'API
  (CLI11, nlohmann/json). Lo controlla la prova `architettura.nucleo_senza_stampa`.
- Riceve dati gia' controllati nella forma (dagli schemi, nell'API) e restituisce risultati. Per questo si
  prova da solo (GoogleTest), senza passare dalla riga di comando.
- **Nessun valore di regola e nessun nome di committente nel codice**: le regole di un territorio stanno
  nei regolamenti, che sono dati (docs/formato-dati.md). Il nucleo sa *come* si applica una regola
  (es. "assistente se a bordo c'e' l'infanzia e il mezzo ha piu' di N posti"), non *quanto* vale N.
- Oggi:

| File | Compito |
|---|---|
| `domain_models.hpp` | Il modello dati (luoghi, regolamenti, festivita', calendari, scuole, committenti (dal K-012 anche privati), contratti, servizi con la domanda (dal K-012 anche NCC), corse con i passaggi, mezzi, rimesse; dal K-008 norme del paese, elenchi e impostazioni delle aziende, persone, assenze, fermi, turni del giorno, rotazioni, servizi fissi, assegnazioni del singolo giorno, blocchi) e le regole del mestiere sulle corse (categoria ricavata: linea, disabili, scolastico, NCC, promiscuo, altro; posti, assistente). |
| `tempo.cpp` | Date e orari (anche oltre le 24:00). |
| `pacchetto.cpp` | Indice dei codici, calendari (eccezioni, sospensioni, calendario di base, calendari e festivita' aggiunti o tolti), persone a bordo. |
| `ricerca.cpp` | Normalizzazione dei nomi e ricerca dei luoghi per nome o codice esterno. |
| `turni.cpp` | Chi fa ogni turno in una data (`Planner`): assegnazione del singolo giorno, poi servizio fisso, poi rotazione; i fatti con le date (assenze, disponibilita', scadenze, fermi, distrazioni, prestiti) che lasciano un turno scoperto o senza mezzo; requisiti di guida; km e tempi fermi di un turno. Il programma non inventa assegnazioni. (K-014) I requisiti che dipendono dal tipo di servizio (`kind_requirements`: usi dei mezzi, autorizzazione del committente, vincolo di destinazione, condizioni dei regolamenti). |
| `controlli.cpp` | Controlli sul contenuto di un pacchetto: ogni problema con il suo tipo, la posizione e i valori, **senza testi** (i testi stanno nel catalogo dell'API). |
| `controlli_turni.cpp` | Controlli sui turni giorno per giorno, anche a cavallo della mezzanotte (K-CTR-054 ... 087); i problemi uguali di molti giorni si raggruppano in periodi. |
| `orari.cpp`, `orari_regole.cpp`, `controlli_orari.cpp` | (K-011) Orari di lavoro: i tempi di ogni persona giorno per giorno per categoria (guida, altre mansioni, disponibilita', reperibilita', interruzione, riposo), quali regimi delle norme valgono e perche' (corsa per corsa, estensione alla settimana, esclusioni tra regimi), il lavoro contato da ogni regime, i tipi di regola (elenco chiuso: ogni tipo sa come si conta, i numeri vengono dai dati) valutati in un mondo certo (errori) e in uno prudente (avvisi con i motivi), il contratto della persona, le scelte dell'azienda e della persona, la chiave del rientro; i controlli sulle regole scritte nelle norme (K-CTR-101 ... 126). Con una richiesta (`HoursRequest`) anche il resoconto per `kzero ore` (`HoursReport`). Dal K-013 quali regimi dipendono dal tipo di servizio delle corse (`kind_dependence`, dai dati: senza i dati particolari tutto quello che ne viene e' riservato, e i problemi lo dicono con `Problem::by_service_kind`) e una funzione sola per la corsa a cui serve un trasferimento a vuoto (`served_trip`). |
| `dimostrazione.cpp` | Alcune regole applicate ai dati di un pacchetto: nessun dato scritto nel codice. |

## Piano 2 - API (`src/api`): il contratto ufficiale

- **Oggi** l'API e' la riga di comando: `kzero <comando> [argomenti] [parametri]`, con uscita in testo o JSON.
- **Domani**, quando ci saranno piu' utenti, la stessa API si potra' offrire via HTTP con una descrizione
  OpenAPI: stessi comandi, stessi documenti JSON. Gli schemi draft-07 di oggi si riusano con piccoli
  adattamenti (OpenAPI 3.1 usa JSON Schema).
- Pezzi:

| File | Compito |
|---|---|
| `comandi.cpp` | **Tabella dei comandi**: nome, descrizione, argomenti, parametri, schema dell'uscita. E' l'unico posto dove si dichiarano. Da qui nascono la lettura dei parametri e `kzero descrivi`. |
| `parametri.cpp` | Lettura dei parametri con CLI11; i controlli e i messaggi in italiano sono di K-Zero. |
| `errori.cpp` | Catalogo dei codici K-ERR e presentazione degli errori in testo o JSON. |
| `uscita.cpp` | Strumenti comuni: documento JSON con "schema" e "schema_versione", arrotondamenti, orari. |
| `comando_*.cpp` | Un file per comando: chiede i risultati al nucleo e li presenta in testo o JSON. `comando_giornata.cpp` mostra i turni di un giorno (tutti, di una persona o di un mezzo) e non mostra nulla se il pacchetto ha errori nei dati; `comando_ore.cpp` (K-011) mostra gli orari di lavoro di un giorno o di un periodo, una persona alla volta (il testo si scrive dal JSON della persona). Gli errori del piano degli orari non bloccano ne' la giornata ne' gli orari. |
| `vista.cpp` | Strumenti comuni ai comandi che mostrano i dati (dimostrazione, giornata): filtro delle classi dei dati, etichette, numeri e durate in italiano. |
| `esegui.cpp` | Dalla riga di comando all'esito: prepara tutto in memoria (`esegui`), poi lo scrive (`consegna`). Cosi' un errore non lascia mai un risultato a meta'. |
| `formato_dati.cpp` | Elenco dei file di un pacchetto, con lo schema e le versioni che il programma sa leggere. |
| `lettore.cpp` | Lettura di un pacchetto: dal K-009 lettura blindata (solo file normali dentro la cartella, mai oltre il massimo di byte), poi JSON valido e senza campi ripetuti, schema e versione giusti, rispetto dello schema; poi la conversione nel modello del nucleo e quanti valori personali e particolari ci sono. Un file con un errore di forma non viene usato. |
| `riservatezza.cpp` | (K-009) Classe del dato in cui sta una posizione e posizione da mostrare: di base non entra mai in un dato personale o particolare, si ferma all'elemento comune piu' vicino (un'assenza diventa la persona). Regola ricavata dalle classi degli schemi. Legame con i luoghi sensibili (`LegameSensibile`, K-009 aggiunta 1; dal K-012 anche i committenti privati): la regola sola usata da controlla, dimostrazione, giornata e dalla cassaforte. (K-014) Il perche' dei turni scoperti o senza mezzo senza i dati particolari (`motivo_dei_turni_riservato`, `turni_senza_motivo`): lo stesso testo per ogni causa. |
| `validazione.cpp` | Controllo con gli schemi (libreria valijson) e messaggi della libreria tradotti in italiano. |
| `schemi_dati.cpp` | Schemi dei file di dati incorporati nel programma e **classe di ogni campo** (comune, personale, particolare), letta dalla marcatura dello schema. |
| `controlli_catalogo.cpp` | Catalogo dei controlli K-CTR: codice, livello, cosa rivela il problema in se' (nessun dato, un dato personale o particolare), descrizione e testo dei messaggi (unico posto da tradurre). Presenta i messaggi seguendo le classi dei dati e i luoghi sensibili. |
| `controllo.cpp` | Controllo completo di un pacchetto: forma, poi contenuto; ordine stabile dei problemi; i problemi che rivelano un dato particolare si contano soltanto, se non si chiede di vederli. Dal K-010 i controlli che hanno bisogno di una classe che l'utente non puo' aprire sono "non verificati", mai fatti su dati mancanti. |
| `classi.cpp` | (K-010) La regola delle classi su un documento: conta i valori personali e particolari, li separa dai file normali e li riunisce, legati per codice dell'elemento (mai per posizione). Una regola sola per contare, chiudere e riaprire. |
| `cripto.cpp`, `password.cpp`, `unicode.cpp` | (K-010) Le sole primitive di libsodium usate dal programma (cifratura autenticata, Argon2id, buste per chiave pubblica, autenticazione, memoria protetta); regole della password (NIST SP 800-63B-4, elenco delle password comuni) e normalizzazione Unicode NFC con utf8proc. |
| `cassaforte.cpp`, `cassaforte_formato.cpp` | (K-010) La cassaforte: utenti, permessi, chiavi dei dati e cambio delle chiavi; il formato byte per byte (docs/cassaforte.md). |
| `registro.cpp`, `disco.cpp` | (K-010) Il registro degli accessi (voci concatenate, cifrato, legato alla cassaforte); scritture atomiche e blocco della cartella. |
| `accesso.cpp`, `chiusura.cpp` | (K-010) Aprire un pacchetto chiuso per un comando (utente, password, voce nel registro prima dei dati, visibilita' dai permessi); chiudere un pacchetto e riaprirlo per controllare che i dati siano identici. |
| `comando_cassaforte.cpp` | (K-010) I comandi della cassaforte: chiudi, richiudi, esporta, cassaforte, registro, aggiungi-utente, togli-utente, password, conservazione, cancella. |

- **I file incorporati**: gli schemi di `docs/schema` e il pacchetto dimostrativo `esempi/demo` entrano
  nel programma quando si compila (`cmake/incorpora.cmake`). Cosi' `kzero` controlla i dati sempre con
  gli schemi con cui e' stato compilato e `kzero dimostrazione` funziona da qualunque cartella.
- I futuri **lettori di file esterni** (es. il file alunni del consorzio, i dati aperti GTFS) staranno in questo
  piano: trasformeranno quei file nel formato K-Zero.

## Piano 3 - Adattatori

- **Interfaccia per persone** (programma per Windows da scaricare, con un suo mandato) ed eventuale **adattatore MCP**
  per gli assistenti AI.
- **Nessuna logica negli adattatori**: chiamano i comandi dell'API e mostrano i risultati. Se serve una
  funzione nuova, nasce come comando dell'API; cosi' nessuna funzione e' raggiungibile solo da un menu.
- Un agente AI non ha bisogno di aspettare l'adattatore MCP: **puo' gia' usare direttamente la riga di
  comando con il JSON**, come spiegato qui sotto.

## Come un agente AI usa kzero oggi

1. `kzero descrivi --formato json`: elenco di comandi, parametri, schemi, codici di uscita e di errore.
2. Lancia il comando con `--formato json` e legge l'uscita standard: un documento JSON con
   `"schema"` e `"schema_versione"`. Lo schema e' il file `docs/schema/<schema>.v<schema_versione>.schema.json`.
3. Se il codice di uscita non e' 0, il risultato e' vuoto e l'uscita di errore contiene un documento
   `errore`: codice `K-ERR-nnn` (spiegato in docs/errori.md), messaggio, posizione quando esiste.
4. Di base l'uscita (testo e JSON) non contiene dati personali (al loro posto c'e' il codice
   dell'elemento) ne' particolari (al loro posto c'e' `"riservato"`). Un agente AI deve usare il JSON.
5. Per i dati: `kzero controlla <cartella> --formato json` controlla un pacchetto e dice cosa correggere,
   con codice K-CTR e posizione (file + percorso JSON); i problemi dei turni ripetuti in molti giorni hanno i
   giorni nel campo `giorni`. Di base i problemi sui dati sulla salute sono solo contati (`problemi_riservati`) e le
   posizioni si fermano ai dati comuni (`posizione_completa_con` dice quale parametro mostra quella completa).
6. Con un pacchetto chiuso (K-010) ogni comando che lo legge vuole `--utente <codice>` e la password; un'AI non la
   deve avere (di base un assistente AI non riceve ne' nomi ne' dati sulla salute): lavora sulle uscite che la persona le
   passa, o su un pacchetto "solo numeri".
   L'uscita JSON dice cosa l'utente ha potuto aprire (`cassaforte`) e quali controlli non sono verificati.
7. Per un giorno: `kzero giornata <cartella> --data AAAA-MM-GG --formato json` (con `--persona` o `--mezzo` per
   una sola persona o un solo mezzo) da' i turni del giorno con chi li fa, da dove viene l'assegnazione, il mezzo,
   corse e attivita' in ordine, tempi fermi, km e i turni scoperti o senza mezzo con il motivo. Se il pacchetto ha
   errori nei dati l'errore K-ERR-018 rimanda a `kzero controlla`.

## Regole del contratto

Sono elencate in CLAUDE.md ("Principi uomo + AI") e valgono per ogni comando futuro. In sintesi:
un solo eseguibile con comandi; `--formato testo|json`; schema e versione in ogni JSON; errori con codice
fisso; codici di uscita 0 riuscito, 1 dati, 2 parametri, 3 interno; stesso input = stesso output
(niente orologio ne' caso, date solo con `--data`); nessun file scritto senza `--scrivi`; dati personali
solo con `--includi-dati-personali`, dati particolari solo con `--includi-dati-particolari`.

Dal K-010, con la cassaforte, tre eccezioni e nessun'altra: l'**orologio** (l'ora delle voci del registro e la scadenza
della data di conservazione), il **caso** (chiavi, sali e nonce: le uscite restano uguali, i byte della cassaforte no) e le
**scritture del registro** senza `--scrivi` (ogni apertura scrive una voce). I comandi che cambiano la cassaforte (utenti,
password, dati, conservazione, cancellazione) scrivono la cartella del pacchetto con `--scrivi`, anche quando e' la stessa
da cui leggono. I parametri `--includi-dati-*` valgono solo entro i permessi dell'utente (altrimenti un errore K-ERR); i
luoghi sensibili seguono i permessi, non i parametri. La password non e' mai un parametro: si chiede al terminale senza eco,
oppure si legge dall'ingresso standard con `--password-da-ingresso`.

Scelte di dettaglio:
- il comando e' sempre il primo argomento; non c'e' `--help`: la descrizione completa e' `kzero descrivi`
  (un'unica fonte, in italiano). Chi scrive `--help` riceve un errore che indica `kzero descrivi`;
- con `--formato json` scritto da qualche parte, anche una riga di comando sbagliata riceve l'errore in JSON;
- se la forma di un'uscita JSON cambia, cambia `schema_versione` e si aggiunge un nuovo file di schema;
- gli elenchi escono in ordine fisso (alfabetico, o per codice);
- nel JSON i valori calcolati sono arrotondati: km al metro, euro al centesimo;
- un orario oltre le 24:00 resta tale anche in uscita ("25:30").

## Sicurezza

Spiegazione completa in [sicurezza.md](sicurezza.md): cosa protegge K-Zero e da chi, le misure, i rischi che restano,
i consigli per chi lo usa, cosa fare in caso di violazione. In breve, principi decisi dal supervisore (12/10 e 18/10), scritti
anche in CLAUDE.md:
- il programma lavora sul PC e **nessun comando usa la rete** (due prove: simboli del programma compilato e sorgenti).
  Quando servira' (es. le distanze da Google Maps), lo fara' solo un adattatore separato, acceso con un parametro
  esplicito, che manda fuori il minimo (coordinate, mai nomi o altri dati delle persone), con un suo mandato;
- nessun file scritto senza `--scrivi` (tranne il registro degli accessi, K-010); nessun dato personale nei registri
  del programma (il registro degli accessi dice chi ha aperto cosa, mai i dati);
- ogni dato in ingresso e' controllato con limiti di buon senso e la **lettura e' blindata** (K-009): solo file normali
  dentro la cartella del pacchetto, mai oltre 256 MB anche se il file non dice la dimensione o cresce; file malformati
  rifiutati senza blocchi;
- ogni uscita, messaggio, posizione o registro passa dal **filtro delle classi** dei dati (sotto e docs/controlli.md);
- si conserva il minimo dei dati personali (nessun campo per codice fiscale, telefoni, mail, cause di assenza);
- **prove di sicurezza** a ogni proposta: sanitizer (AddressSanitizer, UndefinedBehaviorSanitizer) e fuzzing
  (libFuzzer); **protezioni del compilatore** nelle versioni di rilascio; **componenti fissati** ed elencati in
  [componenti.cdx.json](componenti.cdx.json) (CycloneDX);
- le domande all'utente le fara' il programma stesso, a scelta multipla, senza AI; un assistente AI e' solo un aiuto
  facoltativo e di base non riceve ne' nomi ne' dati sulla salute;
- (K-010) i dati personali e particolari veri stanno solo nella **cassaforte** del pacchetto (docs/cassaforte.md): cifrati
  con libsodium, una password per utente, permessi per classe che decidono cosa si puo' decifrare, un registro degli
  accessi concatenato e cifrato. I dati decifrati restano in memoria (protetta, dove la libreria lo permette); l'unica
  uscita in chiaro su disco e' quella chiesta apposta (`kzero esporta --scrivi`).

## Dati: tre classi, due livelli

Ogni campo dei file di dati e' marcato nel suo schema con una **classe** (spiegazione completa in
docs/formato-dati.md):
- **comune**: fermate pubbliche, scuole e centri, orari, calendari, conteggi senza esigenze, mezzi,
  aziende, committenti, contratti, regolamenti, tutti i codici;
- **personale**: nomi, indirizzi, case, punti porta a porta (sono la casa del passeggero), ricoveri presso
  la casa di una persona, motivi scritti a mano; dal K-008 i dati delle persone (tranne codice, azienda, presa
  servizio, mezzo abituale, ipotetica e prestiti), il tipo e i giorni delle assenze, le note di rotazioni, servizi
  fissi e assegnazioni;
- **particolare** (salute): esigenze di persone e gruppi, tipo di servizio e di gruppo ("disabili"),
  conteggi di carrozzine, competenze richieste da un servizio o da un turno.

Il filtro delle uscite parte dalla marcatura dello schema ed e' **uguale per testo e JSON**: di base un dato
personale e' sostituito dal codice del suo elemento, un dato particolare diventa `"riservato"`; con
`--includi-dati-personali` e `--includi-dati-particolari` compaiono. Vale anche nei messaggi dei controlli
(es. il nome di una casa, il valore sbagliato di un campo particolare). Un campo senza marcatura vale come
particolare, per prudenza; una prova controlla che tutti i campi siano marcati. Dal K-009 vale anche per il problema
intero (un problema che rivela un dato sulla salute di base si conta soltanto), per le posizioni (mai dentro un dato
personale o particolare: si fermano all'elemento comune piu' vicino; un'assenza diventa la persona, marcatura
`x-kzero-titolare`) e per i **luoghi sensibili** (il legame tra un passeggero, o la sua casa, e un centro diurno e'
riservato senza i dati particolari; una regola sola, `LegameSensibile` in riservatezza.hpp).

Dal K-010 la stessa marcatura decide cosa va nella **cassaforte**: chiudendo un pacchetto (`kzero chiudi`) ogni valore
personale o particolare lascia i file normali e va nella cassaforte, legato al suo elemento per codice; aprendolo, i valori
che l'utente puo' decifrare si riuniscono in memoria e la forma si controlla sui dati uniti. Quello che l'utente non puo'
aprire vale come presente ma non leggibile, e i controlli che ne hanno bisogno escono come "non verificati". Le uscite
prendono la visibilita' dai permessi e dai parametri insieme.

Il **livello** del pacchetto ("solo numeri", consigliato, oppure "completo") dice se ci possono essere
passeggeri uno per uno e case; un pacchetto "solo numeri" che li contiene e' un errore (K-CTR-019).
Il formato non ha campi per codice fiscale, telefoni, mail: K-Zero non li conserva mai.

## Il regolamento come dato

Le regole di un territorio o di un committente (attese massime, distanze, alunni minimi, assistente,
adulto alla fermata, passeggeri in piedi...) stanno in un **regolamento**, un file di dati con la fonte di
ogni valore. Ogni contratto indica il suo regolamento. Il pacchetto demo ne ha due: le regole pubbliche
della Provincia di Trento e un regolamento inventato con valori diversi; le prove controllano che lo stesso
dato dia esiti diversi con i due regolamenti. Cosi' K-Zero puo' lavorare in Sicilia come in Germania senza
cambiare il codice.

## Come si aggiunge un comando

1. Una voce nella tabella di `src/api/comandi.cpp` (nome, descrizione, parametri, schema).
2. Un file `src/api/comando_<nome>.cpp` che chiede i risultati al nucleo e prepara testo e JSON.
3. Lo schema `docs/schema/<schema>.v1.schema.json`.
4. Eventuali nuovi errori nel catalogo (`src/api/errori.cpp`) e in docs/errori.md.
5. Le prove in `tests/CMakeLists.txt` (`kzero_prova(...)`).

`kzero descrivi` si aggiorna da solo; le prove controllano che docs/errori.md, docs/controlli.md,
docs/formato-dati.md e docs/schema coincidano con il programma.

## Come si aggiunge un controllo sui dati

1. Un valore nuovo in `Check` (src/nucleo/controlli.hpp) e il controllo in `controlli.cpp` (senza testi).
2. Una voce nel catalogo (`src/api/controlli_catalogo.cpp`) con il codice K-CTR successivo, il livello e
   il testo dei messaggi; la riga in docs/controlli.md.
3. Una prova nel nucleo (tests/test_controlli.cpp) e, per un errore, un pacchetto rotto in
   tests/CMakeLists.txt (`kzero_rotto(...)`: il demo con un solo valore cambiato).

## Prove

| Etichetta | Cosa controlla |
|---|---|
| `nucleo` | Regole del mestiere, date e orari, ricerca e normalizzazione, calendari, persone a bordo, ogni controllo sul contenuto, dimostrazione; dal K-008 chi fa il turno (rotazioni di ogni forma, servizi fissi, assegnazioni, fatti con le date, giorni del demo controllati a mano) e ogni controllo sui turni; dal K-011 ogni tipo di regola sugli orari con i casi al limite (esattamente il limite e un minuto oltre, mezzanotte, cambio di settimana, riposi frazionati, compensazioni), la scelta dei regimi, i conteggi, i livelli e la chiave del rientro (GoogleTest, con i dati del pacchetto demo). |
| `api` | Tabella dei comandi, cataloghi degli errori e dei controlli, controlli sulla forma dei file, file costruiti per rompere il programma, limiti di buon senso negli schemi, messaggi in italiano, classi dei dati e loro filtro, comando giornata (scheda scuola del committente ricavata dall'uscita) (GoogleTest). |
| `programma` | Lanciano `kzero` due volte: codice di uscita, K-ERR per i parametri sbagliati, JSON valido, uscite identiche byte per byte, dati personali e particolari assenti di base e presenti con il parametro, `kzero controlla` sui pacchetti di esempio e su un pacchetto rotto per ogni errore (il demo con un solo valore cambiato), due regolamenti con esiti diversi. |
| `documentazione` | docs/errori.md, docs/controlli.md, docs/formato-dati.md e docs/schema coincidono con `kzero descrivi`; la tabella delle lettere di docs/formato-dati.md coincide con il programma. |
| `architettura` | Il nucleo non stampa e non legge la riga di comando. |
| `sicurezza` | (K-009) Niente rete nel programma compilato e nei sorgenti; protezioni del compilatore nella versione di rilascio per Linux; l'elenco dei componenti (docs/componenti.cdx.json) coincide con le versioni e le impronte fissate. |
| `sicurezza` (K-010) | Prove interne della cassaforte e del registro: chiusura e riapertura identica, permessi, cambio delle chiavi, ogni byte cambiato, chiave pubblica sostituita, permesso alzato a mano, utente tolto, catena del registro, voce non scritta = nessun dato, interruzioni a meta' (processo fermato di colpo), comandi insieme, dati decifrati mai su disco; unione per codice dopo riordini e cancellazioni; "non verificato" su centinaia di pacchetti; (aggiunta 1) dati legati a un luogo sensibile decifrati solo con tutti e due i permessi, provato sul file anche dopo un cambio delle chiavi, cassaforte finta fatta da chi ha solo i file, registro riscritto prima dell'ancora, avvisi sulle password; sul programma vero la copia congelata della cassaforte versione 1 (tests/esempi/k010-cassaforte) per i tre tipi di utente, in testo e JSON; (K-013) la prova per differenza degli orari senza i dati particolari (due pacchetti uguali tranne il tipo di un servizio, per ogni coppia di tipi: controlla, ore e giornata uguali tranne i totali) e gli orari del titolare e dell'utente con i soli dati personali. |
| `deposito` | (K-015) Il deposito e' pubblico: in nessun file di testo del deposito (fuori da build*/ e .git) compare uno dei quattro nomi che non si scrivono (CLAUDE.md, Confini; il controllo ne conosce solo le impronte SHA-256) e fuori da tests/ ed esempi/ non ci sono file dei formati dei dati ne' manifesti di un pacchetto; lo strumento del controllo provato con una lista finta di nomi inventati, un caso per ogni regola. |
| `fuzz` | (K-009) Ogni file del demo nel suo bersaglio di fuzzing e ogni caso gia' trovato dal fuzzing (tests/fuzz/casi), rigiocati come prove normali. Il fuzzing vero (libFuzzer, `-DKZERO_FUZZ=ON`) gira nel controllo GitHub per un tempo fisso e in locale a lungo (CLAUDE.md). |
| `schemi` | Con lo strumento standard check-jsonschema: schemi validi per draft-07; ogni uscita JSON delle prove e ogni file dei pacchetti di esempio rispetta lo schema che dichiara. |
| `scala` | Un pacchetto grande generato con un seme fisso (600 mezzi, 5.000 fermate, 10.000 corse; dal K-008 1.320 persone, 1.200 turni, rotazioni di lunghezze diverse, un anno di festivita') passa `kzero controlla` senza errori e `kzero giornata` su una data; dal K-011 con le leggi sugli orari del demo e `kzero ore` di tutti per un mese; dal K-012 misto (610 mezzi con 10 di scorta, 9.800 corse: linee anche oltre i 50 km, scolastico, disabili verso centri diurni sensibili, corse promiscue, NCC con pullman e con auto, OUT o tachigrafo per tipo e per servizio, prestiti di persone e di mezzi): resta un solo avviso, la media su 4 mesi del D.Lgs. 234 che esce dal periodo. Tempo e memoria si annotano nel DIARIO, non sono una soglia della prova. |

## Limiti noti

- Classi dei dati: i nomi di scuole e centri, dei servizi e dei gruppi sono dati comuni (come chiede il
  formato) ma sono testi liberi: un nome come "Centro diurno" o un centro di tipo "terapie" lasciano intuire
  un dato sulla salute. Chi scrive i dati deve evitare nomi che rivelano dati personali o particolari
  (docs/sicurezza.md, con esempi).
- Ricerca: le lettere di altri alfabeti (greco, cirillico...) non vengono ricondotte all'alfabeto latino.
- Ora legale: gli orari sono orari dell'orologio; nei due giorni del cambio una corsa che attraversa le
  02:00-03:00 dura un'ora in piu' o in meno di quanto risulta (docs/formato-dati.md). Dal K-011 un turno che attraversa
  il cambio (date nelle norme del paese) da' un avviso (K-CTR-103): le sue durate vanno controllate a mano.
- I messaggi della libreria di validazione sono in inglese e vengono tradotti: un messaggio che la
  traduzione non conosce compare come "non rispetta lo schema (...)" con il testo originale.
- Windows (mandato del programma per Windows): argomenti con lettere accentate e a capo "CR LF" sono da sistemare e
  provare li'; anche le prove della lettura blindata e le protezioni del compilatore per Windows (docs/sicurezza.md) e,
  per la cassaforte, la password dal terminale senza eco, il blocco della cartella, la sostituzione atomica dei file e la
  memoria protetta.
- Stesso input = stesso output vale a parita' di programma e di sistema. Tra sistemi diversi i valori
  calcolati con decimali potrebbero differire all'ultima cifra: per questo il JSON li arrotonda.
- `kzero giornata` e `kzero ore` rifanno tutti i controlli del pacchetto prima di mostrare un giorno (con errori nei
  dati non mostrano nulla): con 600 mezzi, 1.200 turni e 1.320 persone i controlli di un anno, orari compresi,
  richiedono circa 11 secondi; gli orari di tutte le persone per un anno intero si preparano tutti in memoria prima di
  scriverli (circa 1,4 GB in testo, 3,4 GB in JSON: meglio un mese alla volta, DIARIO).
