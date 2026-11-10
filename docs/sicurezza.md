# Sicurezza e protezione dei dati in K-Zero

K-Zero trattera' dati di minori e dati sulla salute (persone con disabilita'), cioe' dati "particolari" (GDPR art. 9).
Il supervisore, 12/10 e 18/10: "il software dev'essere blindato"; "creare un software fallace dal punto di vista privacy e
sicurezza, trattando dati sensibili, e' un suicidio". Questo documento dice cosa protegge K-Zero, da chi, con quali
misure, quali rischi restano e cosa fare se qualcosa va storto.

**Non e' un parere legale.** Le conclusioni legali (chi e' titolare, chi responsabile, se serve la valutazione
d'impatto, i tempi di una notifica) le conferma il responsabile della protezione dei dati (RPD, in inglese DPO) del
titolare dei dati: per la Provincia autonoma di Trento **idprivacy@provincia.tn.it**. Riferimenti: GDPR artt. 5
(principi), 9 (dati particolari), 25 (protezione fin dalla progettazione e per impostazione predefinita), 28
(responsabile del trattamento), 32 (sicurezza), 33 (violazioni; c. 2: il responsabile informa il titolare senza
ingiustificato ritardo), 35 (valutazione d'impatto); Linee guida EDPB 4/2019 sulla protezione dei dati fin dalla
progettazione e per impostazione predefinita.

**Il caso piu' stretto.** Il titolare dei dati degli alunni e' con ogni probabilita' la Provincia; le aziende li
ricevono dal consorzio senza un accordo scritto noto. K-Zero quindi si progetta per il caso piu' stretto: **nessun dato
personale esce dal PC** di chi pianifica. Il programma lo potra' usare chiunque, anche in aziende con piu' persone.

## 1. Cosa tratta K-Zero, dove e perche' (base per la valutazione d'impatto, art. 35)

La valutazione d'impatto la fa il titolare; questa sezione gli da' i fatti sul programma.

| Domanda | Risposta |
|---|---|
| Perche' (finalita') | Pianificare il trasporto: servizi di linea, scolastici, per disabili, NCC (noleggio con conducente), promiscui; turni di autisti e assistenti; mezzi. |
| Quali dati | Comuni: fermate pubbliche, scuole e centri, orari, calendari, mezzi, contratti, regolamenti, conteggi. Personali: dei passeggeri nome, casa, punto porta a porta, motivi scritti a mano (solo nel livello "completo"); di autisti e assistenti nome, casa, ruoli, rapporto e ore, disponibilita', abilitazioni, autorizzazioni, competenze, preferenze, assenze (tipo e giorni, mai la causa); dal K-011 anche contratto collettivo, ore previste e massime, scelte sugli orari e chiave del rientro della persona, giornate a chiamata, e quale legge sugli orari vale per la persona quando dipende dal rapporto o dal contratto collettivo. Particolari (salute): esigenze dei passeggeri (carrozzina, ausili piegati, assistente necessario), tipo "disabili" di servizi e gruppi, conteggi di carrozzine e ausili, competenze richieste da un servizio o da un turno; dal K-011 anche il tipo di servizio come motivo di un avviso sugli orari ("c'e' un motivo riservato"). Elenco completo, campo per campo: [formato-dati.md](formato-dati.md). |
| Di chi | Alunni (minori), persone con disabilita' e loro accompagnatori, autisti e assistenti delle aziende. |
| Quali dati mai | Codice fiscale, telefoni, mail, data di nascita, idoneita' medica, cause delle assenze: il formato non ha campi per scriverli (un campo cosi' aggiunto a mano e' un errore). |
| Quanto | Il minimo: il livello "solo numeri" (consigliato) ha solo conteggi per fermata, niente nomi, case, porta a porta, motivi. |
| Dove | Solo nella cartella del pacchetto, sul PC di chi pianifica. Dal K-010 i dati veri stanno cosi': i **file normali** hanno solo i dati comuni; tutti i valori personali e particolari stanno nella **cassaforte** (`cassaforte.kzc`), cifrata; accanto c'e' il **registro degli accessi** (`registro.kzr`), cifrato. Il programma non usa la rete e non scrive file senza `--scrivi` (sola eccezione: la voce del registro a ogni apertura). |
| Chi li vede | Solo gli **utenti della cassaforte**, ognuno con la sua password e i suoi permessi (dati personali, dati particolari, o tutti e due); ognuno decifra solo le classi dei suoi permessi. Le uscite di base non contengono dati personali ne' particolari (sezione 3). |
| Per quanto | Fino alla **data di conservazione** scelta da chi chiude il pacchetto (si puo' cambiare): passata, ogni apertura lo ricorda; `kzero cancella` distrugge la cassaforte e il registro. Il registro dura quanto la cassaforte. |
| Chi decide | Titolare dei dati degli alunni: con ogni probabilita' la Provincia (da confermare con il suo RPD). L'azienda che pianifica per conto del committente e' responsabile del trattamento (art. 28) per quei dati, e titolare dei dati dei propri dipendenti. |
| Chi ha aperto | Il registro degli accessi: ogni apertura e ogni azione (utente, data e ora, comando, classi aperte e mostrate, mai i dati); lo legge ogni utente con `kzero registro`. |

`kzero controlla <cartella>` dice in ogni momento quali dati contiene un pacchetto: il livello e quanti valori
personali e particolari ci sono (file per file con `--includi-dati-personali`); `kzero cassaforte <cartella>` dice quanti
valori ci sono nella cassaforte, chi sono gli utenti e con quali permessi, fino a quando si conservano i dati.

## 2. Da cosa protegge, e da chi

| Chi o cosa | Cosa fa K-Zero oggi | Cosa resta |
|---|---|---|
| **Chi usa lo stesso PC** (un collega, un familiare) | Senza una password della cassaforte vede solo i file normali, con i dati comuni; ogni apertura chiede utente e password e va nel registro. | I file normali (dati comuni, nomi liberi: sezione 4); il PC acceso con il programma aperto; un programma malevolo che legge la tastiera o la memoria. |
| **Un PC o una chiavetta persi o rubati** | La cassaforte e' cifrata (XChaCha20-Poly1305) con chiavi chiuse dalle password (Argon2id, 256 MiB e 3 passaggi: ogni tentativo costa tempo e memoria). | Una password debole si puo' indovinare provando su una copia; le copie in chiaro (la cartella prima della chiusura, le uscite in chiaro) se non sono state cancellate: serve anche la cifratura del disco (sezione 5). |
| **Un utente con meno permessi** (es. solo i dati personali) | Non riceve mai la chiave dei dati particolari, nemmeno quando le chiavi si rifanno; non puo' dare a se' o ad altri permessi che non ha, neanche cambiando il file con un altro programma (sezione 3); i parametri `--includi-dati-*` oltre i permessi sono un errore. | Puo' copiare fuori i dati che legge: e' il suo permesso. |
| **Un utente tolto** | Le chiavi dei dati che aveva si rifanno e i dati si ricifrano: non apre la versione nuova, e il registro si ricifra. | Le copie vecchie che ha tenuto restano leggibili con la sua password di allora; una copia vecchia rimessa al posto di quella nuova non si scopre (sezione 4). |
| **Chi cambia i file della cassaforte con un altro programma** | Intestazione autenticata con una chiave che hanno solo gli utenti, elenchi dei permessi autenticati con le chiavi dei dati: un utente aggiunto da fuori, una chiave pubblica sostituita, un permesso alzato, un parametro abbassato si scoprono e niente si apre (K-ERR-028). Il registro e' a catena e legato alla cassaforte. | Chi ha le chiavi e sa programmare puo' riscrivere il registro (sezione 4). |
| **Un file di dati preparato apposta**, o rovinato | Lettura blindata, limiti di buon senso, controllo con gli schemi, file malformati rifiutati senza blocchi; cassaforte e registro con limiti su ogni lunghezza, parametri assurdi rifiutati prima di calcolare; provato con i sanitizer e con il fuzzing a ogni proposta; protezioni del compilatore (sezione 3). | Rischi residui in sezione 4. |
| **Un assistente AI che legge le uscite** (spesso su server esterni) | Di base le uscite non contengono dati personali (al loro posto il codice) ne' particolari ("riservato"), nemmeno nei messaggi, nelle posizioni o nel solo fatto che ci sia un problema sulla salute (sezione 3). Un assistente AI e' solo un aiuto facoltativo. | Con `--includi-dati-personali` o `--includi-dati-particolari` l'uscita contiene quei dati: non va data a un assistente AI. |
| **Uno sbaglio di chi scrive i dati** | I controlli K-CTR (docs/controlli.md): es. un pacchetto "solo numeri" con dei nomi e' un errore (K-CTR-019); dati personali in chiaro in un pacchetto che non dice "dati inventati" sono un avviso a ogni lettura (K-CTR-096). | I nomi liberi (servizi, corse, turni, note, codici) possono rivelare un dato sulla salute: sezione 5. |
| **La rete** | Nessun comando usa la rete; due prove lo controllano (sezione 3). | Il futuro adattatore delle distanze (es. Google Maps) sara' l'unica eccezione: un programma separato, acceso con un parametro esplicito, che manda fuori solo coordinate (mai nomi o altri dati delle persone), con un suo mandato. |
| **Chi legge il deposito pubblico** (chiunque, dal 5/11/2026) | Nel deposito solo codice, documenti e dati inventati o pubblici: nessun dato vero, nemmeno cifrato (CLAUDE.md, Confini); reti: .gitignore, la prova `deposito` e il controllo GitHub di titolo e commit delle proposte (sezione 3). | La storia prima del 5/11 contiene i nomi tolti dal K-015; le reti non riconoscono un pacchetto vero (sezione 4). |
| **Un componente esterno compromesso** | Poche librerie, con versione e impronta fissate (libsodium anche con la firma dell'autore verificata); elenco dei componenti in un formato standard (sezione 3). | Le librerie del sistema (libreria C e C++ del compilatore) non sono nell'elenco. |

## 3. Le misure di oggi

**Classi dei dati e livelli** ([formato-dati.md](formato-dati.md)). Ogni campo dei file e' marcato nel suo schema come
comune, personale o particolare; il pacchetto dichiara il livello "solo numeri" o "completo". Un campo senza marcatura
vale come particolare, per prudenza; una prova controlla che tutti i campi siano marcati.

**Filtri delle uscite**, uguali per testo e JSON e per tutti i comandi: di base un dato personale diventa il codice
dell'elemento, un dato particolare diventa "riservato"; si vedono solo con `--includi-dati-personali` e
`--includi-dati-particolari`. Dal K-009 ([controlli.md](controlli.md), "Dati personali e particolari nei problemi"):
- ogni controllo dichiara cosa rivela in se'; un problema che rivela un dato sulla salute di base non si elenca (niente
  codice ne' posizione): una riga sola dice quanti errori e avvisi sono, e contano lo stesso;
- la posizione di un problema non entra mai in un dato personale o particolare: si ferma all'elemento comune piu'
  vicino; un problema su un'assenza indica la persona, non il file delle assenze. Regola ricavata dagli schemi;
- **luoghi sensibili** (es. un centro diurno, segnato da chi scrive i dati): dal K-010 la regola vale per la persona, non
  per il luogo. Il nome, la casa e gli altri dati personali di chi e' legato a un luogo sensibile (chi ci va, chi sale o
  scende li', le case porta a porta toccate da quei giri) valgono come dati particolari **in tutte le uscite**, non solo
  dove compare il centro; il centro si vede sempre. Cosi' la regola regge tra uscite diverse: nessuna lega mai il codice
  della casa al nome senza i dati particolari ([formato-dati.md](formato-dati.md), "Luoghi sensibili"). Nella cassaforte
  quei dati stanno in una sezione a parte che si decifra solo con tutti e due i permessi (personali e particolari): chi
  ne ha uno solo non li puo' decifrare, nemmeno lavorando sul file (aggiunta 1 del K-010). Una regola sola nel codice
  (`LegameSensibile`, src/api/riservatezza.hpp);
- **tipo di servizio** (K-013, K-014): il tipo di un servizio e' un dato particolare, perche' dice chi viaggia (il servizio
  per disabili). Senza i dati particolari non lo lasciano capire ne' le leggi sugli orari che dipendono dal tipo (K-013) ne'
  i requisiti dei turni che dipendono dal tipo (usi dei mezzi, autorizzazione del committente, vincolo di destinazione,
  K-014): i loro problemi si contano soltanto, e di un turno scoperto o senza mezzo si vede il fatto ma non il perche'
  (rischio che resta: sezione 4). Lo controlla una prova per differenza su pacchetti uguali tranne il tipo di un servizio;
- gli errori del programma (K-ERR) non nominano mai un file di soli dati personali.

**Cassaforte dei dati personali** (K-010, [cassaforte.md](cassaforte.md)). I dati veri stanno solo nella cassaforte:
- **tutti i valori personali e particolari** del pacchetto, scelti con le classi degli schemi (la stessa regola con cui
  `kzero controlla` li conta), legati al loro elemento con il codice, mai con la posizione: un elemento tolto o cambiato
  dopo la chiusura e' un errore (K-CTR-094), mai un nome sulla persona sbagliata. I file normali restano leggibili, con
  i soli dati comuni; all'apertura i dati si uniscono in memoria;
- **cifratura** con libsodium (XChaCha20-Poly1305, usata come prescrive la sua documentazione: niente cifratura fatta in
  casa); chiavi dalle password con **Argon2id** (256 MiB, 3 passaggi; parametri scritti nel file, rifiutati se abbassati
  o assurdi, prima di calcolare);
- **piu' utenti con permessi**: dati personali, dati particolari o tutti e due; due chiavi dei dati separate e una
  "fessura" per utente con le sole chiavi dei suoi permessi; i dati di chi e' legato a un luogo sensibile con una chiave
  ricavata da tutte e due. Si danno al massimo i permessi che si hanno; togliere un utente rifa' le chiavi e ricifra i
  dati (lo fa chi ha tutti e due i permessi); non si toglie l'ultimo che legge una classe, ne' l'ultimo con tutti i
  permessi;
- **intestazione autenticata**: utenti, chiavi pubbliche, permessi, parametri e versione sono autenticati con una chiave
  che hanno solo gli utenti (rifatta quando si toglie un utente); chi ha i dati particolari e' autenticato anche con la
  chiave dei particolari, chi ha i personali con quella dei personali: un permesso alzato a mano, anche da un utente
  vero, si scopre prima di consegnare una chiave;
- **password**: mai come parametro ne' in una variabile d'ambiente, dal terminale senza eco (la nuova due volte) o
  dall'ingresso standard con `--password-da-ingresso`; regole di NIST SP 800-63B-4 (almeno 15 caratteri, fino a 256,
  qualunque carattere, rifiutate le password comuni e quelle prevedibili); normalizzazione Unicode NFC (stessa password
  su Linux e su Windows). Nessun recupero: un altro utente con i permessi della persona puo' darle una password nuova;
- **permessi nelle uscite**: `--includi-dati-personali` e `--includi-dati-particolari` valgono solo entro i permessi
  (altrimenti errore K-ERR-023). Per chi non puo' aprire una classe, un controllo o un calcolo che ne ha bisogno e'
  **"non verificato"** e il risultato lo dice: mai un risultato fatto su dati mancanti come se fossero completi (una
  prova confronta i risultati dei tre tipi di utente su 375 pacchetti);
- **memoria protetta**: chiavi, password e testo decifrato in memoria bloccata, con pagine di guardia, esclusa dai core
  dump e azzerata appena finito; mentre la cassaforte e' aperta il programma non scrive "core dump" (Linux). Dati
  decifrati mai su disco ne' in file temporanei: l'uscita in chiaro (`kzero esporta`) e' l'unica eccezione, esplicita;
- **cancellazione e conservazione**: data di conservazione scelta alla chiusura (si puo' cambiare); passata, ogni
  apertura lo ricorda (K-CTR-097); `kzero cancella` distrugge prima le fessure con le chiavi, poi i file, e stampa un
  resoconto da conservare.

**Registro degli accessi** (K-010). Ogni apertura e ogni azione scrive una voce (utente, data e ora UTC dall'orologio del
PC, comando, classi aperte e mostrate, una nota senza dati personali); se la voce non si scrive i dati non si aprono. Voci
a catena (ognuna contiene l'impronta di quella prima) e la cassaforte ricorda l'ultima voce scritta insieme a lei: una
voce tolta, cambiata o spostata si scopre. Cifrato con una chiave che hanno tutti gli utenti; in un file accanto alla
cassaforte, cosi' un comando che legge non riscrive i dati. Scritture atomiche (file temporaneo, poi sostituzione) e
cartella bloccata durante ogni scrittura: due comandi insieme, anche da due PC su una cartella condivisa che lo permette,
non perdono voci e non rovinano i file (provato).

**Minimizzazione.** Nessun campo per codice fiscale, telefoni, mail, data di nascita, cause delle assenze, idoneita'
medica. Il livello "solo numeri" e' quello consigliato.

**Niente rete, provato.** Una prova legge il programma compilato (Linux) e controlla che non importi nessuna funzione di
rete (socket, connect, getaddrinfo e simili) e non sia collegato a librerie di rete; un'altra controlla che nessun file
del codice includa intestazioni di rete o si colleghi a librerie di rete.

**Niente scritture senza `--scrivi`.** Il risultato va sull'uscita standard; nessun comando scrive file senza
`--scrivi <cartella>`, che dice proprio quale cartella si scrive o si cambia (chiusura, uscita in chiaro, utenti,
password, conservazione, cancellazione). Le uniche eccezioni, documentate: la voce del registro a ogni apertura, l'ora
del registro e la data di conservazione dall'orologio del PC, chiavi, sali e nonce dal caso (le uscite restano uguali, i
byte della cassaforte no).

**Limiti di buon senso.** Ogni numero e testo dei file ha un massimo nello schema (una prova lo controlla); un file oltre
256 MB non si legge.

**Lettura blindata dei file** (K-009). Si legge solo un file normale: non un dispositivo (es. `/dev/zero`, che non
finisce mai), una "pipe", una cartella (errori K-CTR-088 e K-CTR-090). Un collegamento (link) si segue solo se porta a un
file normale dentro la cartella del pacchetto (altrimenti K-CTR-089): cosi' un pacchetto preparato apposta non fa
leggere al programma un file qualsiasi del PC. La cartella del pacchetto puo' essere a sua volta un collegamento (es.
una cartella sincronizzata). Mai piu' del massimo di byte, anche quando il sistema non dice la dimensione o il file
cresce mentre lo si legge: si legge fino al massimo piu' uno e ci si ferma. I messaggi non riportano mai il contenuto
del file.

**Prove di sicurezza nel controllo GitHub** (GDPR art. 32 c. 1 lett. d: provare regolarmente l'efficacia delle misure).
A ogni proposta, oltre alle prove normali:
- **sanitizer**: il programma e tutte le prove compilati con AddressSanitizer (errori di memoria: letture e scritture
  fuori posto, memoria usata dopo averla liberata, perdite) e UndefinedBehaviorSanitizer (operazioni dal risultato non
  definito); il primo problema ferma la prova;
- **fuzzing** (prove con dati casuali e deformati, libFuzzer): 32 bersagli, cioe' ogni tipo di file del pacchetto (il demo
  con quel file sostituito: lettura, controlli, messaggi), la riga di comando, date e orari e, dal K-010, il file della
  cassaforte, il testo decifrato delle sue sezioni, il file e il testo del registro, le password (per non calcolare
  Argon2id a ogni caso questi bersagli usano parametri leggeri che il programma kzero non accetta); partono dai file del demo e
  controllano anche la coerenza dei risultati (posizioni mostrate solo su dati comuni, JSON sempre valido, stessa regola
  delle date nel programma e negli schemi). **20 secondi per bersaglio**, piu' bersagli insieme: circa 5 minuti in tutto,
  come le prove normali, per non rallentare ogni proposta; bastano a provare migliaia di file deformati per ogni tipo e a
  fermare un errore nuovo vicino ai dati di ogni giorno. Le ricerche profonde si fanno in locale, per ore (CLAUDE.md);
  ogni guasto trovato si corregge e il file che lo provoca diventa una prova normale (`tests/fuzz/casi`).

Il DIARIO riporta quanto ha girato il fuzzing in locale e i guasti trovati.

**Protezioni del compilatore** (versioni di rilascio per Linux, DECISIONI.md): controllo dello stack, controlli delle
funzioni della libreria C (`_FORTIFY_SOURCE`) e C++, eseguibile rilocabile (PIE), tabelle in sola lettura (RELRO
completo), stack non eseguibile. Rendono piu' difficile sfruttare un errore di memoria; una prova le ritrova nel
programma compilato.

**Componenti esterni fissati ed elencati.** CLI11, nlohmann/json, valijson, libsodium (cifratura), utf8proc
(normalizzazione Unicode delle password), l'elenco delle password comuni (SecLists) e (solo per le prove) GoogleTest e
check-jsonschema, con versione e impronta fissate. L'elenco completo, leggibile da un programma, e' in
[componenti.cdx.json](componenti.cdx.json) (formato standard CycloneDX 1.5): nome, versione, provenienza, impronta,
licenza, se serve al programma o solo alle prove. Una prova controlla che coincida con quello che si scarica davvero.
Serve a chi comprera' K-Zero e al regolamento europeo sulla cibersicurezza dei prodotti (Cyber Resilience Act), se
K-Zero sara' venduto.

**Deposito pubblico (K-015).** Il deposito su GitHub e' pubblico dal 5/11/2026: chiunque legge il codice, i documenti,
gli schemi, i pacchetti di esempio e di prova e tutta la storia dei salvataggi. La regola (CLAUDE.md, Confini): nessun
dato vero che non sia gia' pubblico entra nel deposito, nemmeno cifrato in una cassaforte (una password di prova e'
pubblica, e una cassaforte vera nel deposito sarebbe esposta per sempre a chi prova le password); i dati veri restano
solo sul PC del supervisore, fuori dalla cartella del deposito. Vanno bene i dati pubblici (luoghi, linee e fermate del
trasporto pubblico, leggi e regolamenti con la fonte) e quelli inventati. Quattro nomi (dell'azienda, del supervisore,
del consorzio e dell'azienda del trasporto pubblico provinciale) non si scrivono piu' da nessuna parte: al loro posto
ALP e le descrizioni. Tre reti, che aiutano ma non bastano:

- **.gitignore**: i file dei formati dei dati (fogli di calcolo, CSV, PDF, documenti di Word, cassaforte `.kzc` e
  registro `.kzr`), con maiuscole e minuscole, non entrano nel deposito fuori da tests/ ed esempi/.
- **La prova `deposito`** (`ctest -L deposito`, in locale e nel controllo GitHub): in nessun file di testo del deposito
  (fuori da build*/ e .git, anche quelli non ancora salvati) compare uno dei quattro nomi, parola intera, con maiuscole
  e minuscole, quello di due parole anche a capo o con un segno di commento in mezzo, anche nei nomi dei file; fuori da
  tests/ ed esempi/ nessun file di quei formati e nessun manifesto di un pacchetto (`pacchetto.json`). I nomi non sono
  scritti nel controllo: ne conosce solo le impronte SHA-256, e i messaggi dicono file, riga e quale nome (a-d), mai il
  nome.
- **Il controllo GitHub delle proposte** (`Nomi della proposta`): lo stesso controllo sul titolo della proposta e sui
  messaggi dei suoi commit nuovi, anche quando il titolo cambia.

## 4. I rischi che restano, e chi li chiudera'

La cassaforte protegge i dati quando i file sono fermi sul disco, in una copia di sicurezza o su un PC rubato, e da chi
non ha una password; non protegge da tutto. Detto onestamente:

- **Chi conosce una password** apre i dati dei permessi di quell'utente. Una password debole si puo' indovinare provando
  su una copia del file, senza limiti di tentativi: Argon2id rende ogni prova lenta e costosa (circa mezzo secondo e
  256 MiB su un PC normale), non impossibile. Per questo le regole delle password nuove e i consigli della sezione 5.
- **Un PC acceso con il programma aperto.** Mentre un comando lavora, i dati uniti stanno nella memoria del programma:
  chiavi, password e testo decifrato in memoria protetta, azzerati appena finito; ma i dati uniti (nomi, case...) stanno
  nelle strutture normali del programma fino alla fine del comando, e il sistema li puo' mettere nel file di scambio
  (swap). Un programma malevolo sul PC, o un amministratore del PC, puo' leggere la tastiera e la memoria. Non promettiamo
  di piu': per questo la cifratura del disco e un utente per persona (sezione 5).
- **Copie vecchie.** La cartella in chiaro da cui si e' chiuso il pacchetto, le uscite in chiaro (`kzero esporta`) e le
  stampe restano dove sono finche' non si cancellano, e anche dopo la cancellazione i dati possono restare sul disco o
  nelle copie di sicurezza. Le copie vecchie della cassaforte restano cifrate con le password di allora: anche dopo
  `kzero cancella`, e anche presso un utente tolto che ne ha tenuta una copia. Su dischi a stato solido la cancellazione
  sul posto delle fessure non garantisce che i byte vecchi spariscano dal disco.
- **Una copia vecchia rimessa al posto di quella nuova** (cassaforte, registro e file normali di una copia di prima, es.
  da un utente tolto che torna con la sua copia) non si scopre senza un riferimento fuori dalla cartella: i file sono
  autentici, solo vecchi. Il registro e l'ancora della cassaforte scoprono le voci tolte o cambiate, non una copia
  intera di prima.
- **Un utente tolto che aveva ancora accesso alla cartella** e gli stessi permessi di chi lo toglie puo', in teoria,
  preparare il file in modo che gli utenti che non hanno piu' aperto la cassaforte con una password nuova accettino
  chiavi che lui conosce (docs/cassaforte.md, "Cosa non si scopre"). Chi lo ha tolto se ne accorgerebbe aprendo.
- **Un utente tolto che conosceva la password di un collega** (scritta, detta, uguale a un'altra) apre la fessura del
  collega e poi la busta con le chiavi nuove: per la cassaforte e' il collega. Rimedio, ricordato dal programma: dopo
  `kzero togli-utente` chi l'ha tolto cambia subito la sua password (K-CTR-099), e a ogni altro utente ogni apertura lo
  ricorda finche' non la cambia (K-CTR-098). Cambiare la password da' anche una coppia di chiavi nuova, cosi' la
  password vecchia non serve nemmeno per i cambi delle chiavi che verranno. Allo stesso modo una password scritta da
  un altro (utente aggiunto, password persa) va cambiata al primo uso (K-CTR-100). Il programma non obbliga: avvisa.
- **Il registro** lo puo' riscrivere tutto chi ha le chiavi e sa programmare (ogni utente le ha, per poterlo scrivere);
  le voci scritte dopo l'ultima modifica della cassaforte (le sole letture) si possono togliere senza che si veda. L'ora e'
  quella dell'orologio del PC, che chi usa il PC puo' cambiare. Un comando su una cartella in sola lettura non apre
  niente, perche' la voce non si puo' scrivere. I tentativi con una password sbagliata non entrano nel registro (senza la
  password non c'e' la chiave per scrivere la voce): il registro dice chi ha aperto, non chi ha provato.
- **Un utente puo' dare ad altri i suoi permessi** (aggiungendo un utente), o copiare fuori i dati che legge: e' quello
  che il permesso gli consente. Il registro lo mostra.
- **I file normali** restano in chiaro: dati comuni (fermate, orari, mezzi, codici, conteggi senza esigenze) e i nomi
  liberi (punto sotto).
- **Nomi liberi.** Nomi di servizi, gruppi, corse, turni, calendari, note e codici sono dati comuni scelti da chi scrive
  i dati: il programma li mostra sempre e stanno nei file normali, in chiaro. Un nome come "Trasporto al centro diurno"
  accanto all'elenco dei passeggeri dice lo stesso di un luogo sensibile. E' responsabilita' di chi scrive i dati
  (sezione 5); un avviso sui nomi liberi arrivera' con un mandato piccolo. Il pacchetto dimostrativo, che ha dati
  inventati ed e' stato scritto prima di questa regola, ha proprio nomi e codici cosi' (il servizio `S-DIS` "Trasporto al
  centro diurno Il Faro", il gruppo "Centro diurno, andata", il turno `T-DIS` "Centro diurno", il calendario `CAL-CENTRO`
  "Centro diurno: lunedi', mercoledi' e giovedi'", la corsa "Centro diurno, giro principale"): restano come esempio di
  cosa evitare, e perche' le prove li usano. Questi nomi dicono "centro diurno" e il nome del servizio dice anche quale
  ("Il Faro", che si legge per esempio nel JSON della giornata): con la regola del K-010 il centro si vede comunque, e
  nome e casa di chi ci va restano dati particolari.
- **Collegamenti tra i codici.** Chi ha i soli dati personali non legge mai nome e casa di chi e' legato a un luogo
  sensibile (sono dati particolari in tutte le uscite), e nemmeno chi ha i soli dati particolari (nella cassaforte si
  decifrano solo con tutti e due i permessi). Restano gli orari e i km, per chi conosce la zona, e i nomi liberi.
- **Turni scoperti o senza mezzo** (K-014). Un turno puo' restare scoperto o senza mezzo per un requisito che dipende dal
  tipo di servizio: un'autorizzazione del committente scaduta (nel demo il regolamento la chiede per i servizi per
  disabili), un uso del mezzo autorizzato solo per una parte del periodo (es. la distrazione di un autobus di linea per
  l'NCC). Senza i dati particolari si vede che il turno e' scoperto o senza mezzo, in quali giorni e con quale persona o
  mezzo previsti, e quello che ne segue nel piano (chi fa i turni, con quale mezzo, i tempi e i km; per esempio senza il
  mezzo non si controlla la patente per quel mezzo, e un turno puo' non essere piu' scoperto); il perche' no, con lo stesso
  testo e la stessa posizione per ogni causa, anche per un'assenza o un fermo. Chi conosce bene gli altri dati puo' a volte
  intuire che il motivo e' un requisito del tipo di servizio: per esempio se la persona prevista quel giorno non ha assenze
  (la giornata della persona le mostra, senza il tipo), se il mezzo non ha fermi (la giornata del mezzo li mostra) o se la
  distrazione del mezzo scade proprio il giorno prima (l'avviso K-CTR-050 si vede sempre). Scelta del supervisore (5/11): nascondere
  il perche' e lasciare visibile il fatto, perche' chi pianifica deve sapere quali turni vanno coperti e quando, anche senza
  i dati sulla salute; nascondere anche il fatto renderebbe il piano inutilizzabile, e un testo a parte solo per le cause del
  tipo direbbe proprio quello. Chi deve vedere il perche' usa `--includi-dati-particolari`, con il permesso di quei dati.
- **Deposito pubblico** (K-015). La storia del deposito prima del 5/11/2026 contiene i quattro nomi tolti dai file di
  oggi (nei documenti, nei commenti e nei messaggi dei salvataggi di allora) e alcuni codici di esempio sostituiti: la
  storia non si riscrive (scelta del supervisore), quindi restano leggibili. Le reti sono solo reti: un pacchetto vero
  e' una cartella di file JSON come il demo, che il .gitignore non riconosce, e la prova `deposito` riconosce solo dal
  manifesto `pacchetto.json` e solo fuori da tests/ ed esempi/; un file vero copiato dentro tests/ o esempi/, un nome
  scritto in un altro modo (spezzato, abbreviato, con lettere simili), un dato vero che non e' un nome (una targa, un
  orario interno) non si riconoscono. Il controllo dei messaggi vale per le proposte, non per i documenti salvati
  direttamente su main, e scopre un nome solo prima dell'unione: un messaggio gia' pubblicato resta nella storia. Le
  impronte dei nomi non sono un segreto: provando le parole (una sigla di tre lettere in un istante) si ritrovano;
  servono solo a non scrivere i nomi in chiaro. La difesa vera e' tenere i dati veri fuori dalla cartella del deposito.
- **"Non verificato".** Chi non puo' aprire una classe vede meno controlli e una giornata "non verificata" in alcune
  parti: e' voluto (mai un risultato su dati mancanti come se fossero completi), ma chi ha tutti i permessi deve guardare
  anche lui il pacchetto.
- **Uscite copiate altrove.** Quello che l'utente salva, stampa o incolla (es. in un assistente AI) esce dal controllo
  del programma.
- **Memoria.** Un file vicino al massimo di 256 MB, preparato apposta, puo' occupare in memoria diversi GB mentre lo si
  legge. Il limite ferma gli errori e i file sbagliati, non un attacco fatto per esaurire la memoria di un PC piccolo.
  Anche un'uscita grande si prepara tutta in memoria prima di scriverla (con un errore non resta mai a meta'): `kzero ore`
  di un anno intero per oltre 1.000 persone chiede 1,4 GB in testo e 3,4 GB in JSON (DIARIO.md); su un PC piccolo
  conviene chiedere un mese alla volta.
- **Chi ha gia' accesso al PC** puo' cambiare i file mentre il programma li legge, o usare un collegamento fisso ("hard
  link", che il sistema non distingue da un file normale): sono fuori da cio' che il programma puo' controllare.
- **Cartelle condivise in rete.** Il blocco della cartella dipende dal sistema: se non si puo' bloccare, il comando si
  ferma (K-ERR-030) e nessun dato si apre.
- **Windows.** Il programma per Windows non c'e' ancora (mandato a parte): le prove della lettura blindata, delle
  password dal terminale, del blocco dei file, della memoria protetta e dei core dump, e le protezioni del compilatore
  equivalenti (`/GS`, `/guard:cf`, `/DYNAMICBASE`, `/HIGHENTROPYVA`, `/NXCOMPAT`) le fara' quel mandato. Oggi tutto e'
  provato solo su Linux.
- **Le librerie del sistema** (libreria C e C++ del compilatore) non sono nell'elenco dei componenti: le aggiorna il
  sistema operativo.

## 5. Consigli per chi usa K-Zero

- **I dati veri fuori dalla cartella del deposito** (K-015): mai dentro la cartella in cui si scarica il deposito di
  K-Zero, nemmeno per una prova veloce e nemmeno chiusi nella cassaforte; una cartella a parte sul PC, con la cifratura
  del disco. Il deposito e' pubblico.
- **I dati veri solo nella cassaforte.** Chiudere il pacchetto (`kzero chiudi`) prima di scriverci dati veri di persone,
  e subito dopo **cancellare la cartella in chiaro**; lo stesso per un'uscita in chiaro, appena richiusa.
- **Password**: una frase lunga (es. quattro o cinque parole che non stanno insieme), diversa da ogni altra password,
  mai scritta nei file del PC. Nessun recupero: tenere **almeno due utenti con tutti i permessi** (es. il titolare e un
  sostituto), cosi' una password persa si sostituisce (`kzero password --per-utente`).
- **Un utente della cassaforte per persona**, con i soli permessi che servono (es. l'ufficio i soli dati personali).
  Chi lascia l'azienda si toglie subito (`kzero togli-utente`), e poi tutti cambiano la password (il programma lo
  ricorda: K-CTR-098, K-CTR-099). Chi riceve un utente nuovo o una password nuova scritta da un altro la cambia al primo
  uso (K-CTR-100).
- **Un utente per persona** anche sul PC, con la sua password, e il blocco dello schermo quando ci si allontana.
- **Cifratura del disco** del sistema operativo, che **resta necessaria** anche con la cassaforte (copie in chiaro, file
  normali, file di scambio): su Windows BitLocker (Pro) o "Crittografia dispositivo" (Home); su Mac FileVault; su Linux la
  cifratura del disco all'installazione.
- **Copie di sicurezza** regolari, su un supporto separato e cifrato, e ogni tanto la prova di rimetterle a posto. Ora
  contengono la cassaforte cifrata: copiare sempre insieme cassaforte, registro e file normali; una copia si apre con le
  password di quando e' stata fatta.
- **Data di conservazione** realistica alla chiusura; quando i dati non servono piu', `kzero cancella` e cancellare anche
  le copie.
- **Guardare il registro** ogni tanto (`kzero registro`): aperture che non si riconoscono vanno capite.
- **Livello "solo numeri" quando basta**: per i turni, i mezzi, i giri bastano i conteggi per fermata. I nomi e le case
  solo quando servono davvero.
- **Nomi liberi senza dati sulla salute.** Da evitare: "Trasporto al centro diurno", "Disabili Pradello", "Corsa
  carrozzina Rossi", "Turno terapie", una nota "Mario deve andare in dialisi", un codice `S-DIS`. Meglio: "Servizio Valfonda
  3", "Andata Il Faro" (se il luogo non e' sensibile), "Turno 4"; il tipo "disabili" va nel campo apposta, che e' gia'
  riservato.
- **Luoghi sensibili segnati**: un centro diurno, di terapia o di riabilitazione con `"sensibile": true` (luoghi.json,
  versione 2). Un doposcuola di solito no. Il porta a porta: la scelta `porta_a_porta_sensibile` del manifesto (versione
  2) e' dell'azienda ([formato-dati.md](formato-dati.md)).
- **Non mandare pacchetti** per mail o su chiavette se non chiusi; se serve, un pacchetto chiuso (la password detta a voce,
  mai nello stesso messaggio) o un pacchetto "solo numeri".
- **Assistenti AI**: dare solo le uscite di base (senza `--includi-dati-personali` e `--includi-dati-particolari`), mai i
  file di un pacchetto in chiaro ne' le password.
- **Programma**: scaricarlo solo dalla fonte ufficiale e tenerlo aggiornato.

## 6. Cosa fare in caso di violazione

Una violazione e' per esempio: un PC o una chiavetta con un pacchetto rubati o persi, un pacchetto o un'uscita con i nomi
mandati alla persona sbagliata, un'uscita con i dati personali incollata in un servizio esterno, una password della
cassaforte scoperta, aperture nel registro che nessuno riconosce.

1. **Fermare il danno**: cambiare le password (del PC e della cassaforte, `kzero password`), togliere l'utente
   coinvolto (`kzero togli-utente`), chiedere a chi ha ricevuto per sbaglio di cancellare, bloccare il PC a distanza se
   si puo'.
2. **Capire cosa e' coinvolto**: `kzero cassaforte <cartella> --utente ...` dice quanti valori personali e particolari
   ci sono nella cassaforte e chi sono gli utenti; `kzero controlla` (su un pacchetto in chiaro, file per file con
   `--includi-dati-personali`) dice il livello e quanti dati ci sono; `kzero registro` dice chi ha aperto la cassaforte e
   quando. Capire anche se sul supporto perso c'erano **copie in chiaro** (la cartella prima della chiusura, uscite in
   chiaro, stampe) e se una **password** puo' essere stata scoperta. Annotare data, ora, cosa e' successo, chi e'
   coinvolto.
   - **Con i dati nella cassaforte** (e nessuna copia in chiaro, password non scoperte e conformi alle regole) i dati
     persi sono cifrati, cioe' incomprensibili per chi non ha una password: e' il caso dell'art. 34 c. 3 lett. a, in cui
     non serve avvisare le persone. **Lo decide il titolare**, che resta tenuto a documentare la violazione e valuta lui
     la notifica al Garante (art. 33). Se una password puo' essere stata scoperta, cambiarla subito su una copia buona e
     trattare la violazione come se i dati fossero in chiaro.
3. **Avvisare**:
   - l'azienda che tratta i dati **per conto di altri** (es. i dati degli alunni per conto della Provincia) informa
     **subito il titolare**, senza ingiustificato ritardo (art. 33 c. 2): e' il titolare che decide se e come notificare
     al Garante, entro 72 ore da quando lo sa (art. 33 c. 1), e se avvisare le persone (art. 34);
   - per i dati di cui l'azienda e' titolare (es. i propri dipendenti) l'azienda valuta lei e, se serve, notifica al
     Garante entro 72 ore e avvisa le persone.
4. **Documentare** la violazione anche quando non va notificata: il titolare documenta ogni violazione (art. 33 c. 5), e
   l'azienda che tratta per conto di altri gli da' i fatti che servono.

I tempi e i modi esatti li conferma il RPD del titolare.
