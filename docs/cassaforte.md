# La cassaforte dei dati personali (K-010)

Da questo mandato i dati personali e particolari veri di un pacchetto stanno **solo nella cassaforte**: un file cifrato,
`cassaforte.kzc`, nella cartella del pacchetto, accanto ai file normali (che restano leggibili, con i soli dati comuni) e
al **registro degli accessi**, `registro.kzr`. Perche' e con quali scelte: [DECISIONI.md](../DECISIONI.md) (K-010). Da
cosa protegge e cosa resta: [sicurezza.md](sicurezza.md). Il formato dei file normali: [formato-dati.md](formato-dati.md).

## In breve

- **Chiudere** un pacchetto in chiaro: `kzero chiudi <cartella-in-chiaro> --scrivi <cartella-nuova> --utente <codice>
  --conservazione AAAA-MM-GG`. Il programma separa i valori personali e particolari dai file normali (con le classi degli
  schemi, la stessa regola con cui `kzero controlla` li conta), li chiude nella cassaforte e scrive tutto in una
  **cartella nuova**; chi chiude diventa il primo utente, con i permessi dei dati personali e particolari, e sceglie la
  password (chiesta due volte). Prima di dire "fatto" riapre il pacchetto con la password e controlla che i dati uniti
  siano identici all'originale. L'originale non si tocca: va cancellato a mano.
- **Usare** il pacchetto chiuso: `kzero controlla <cartella> --utente <codice>`, `kzero giornata <cartella> --data ...
  --utente <codice>`. La password si chiede al terminale (senza eco) o, con `--password-da-ingresso`, dall'ingresso
  standard; mai come parametro ne' in una variabile d'ambiente. Ogni apertura scrive una voce nel registro.
- **Permessi**: ogni utente ha i dati personali, i dati particolari o tutti e due. Decifra solo le classi dei suoi
  permessi; `--includi-dati-*` oltre i permessi e' un errore (K-ERR-023); per le classi che non apre, i controlli e i
  calcoli che ne hanno bisogno sono **"non verificati"** e il risultato lo dice.
- **Correggere** i dati dopo la chiusura: `kzero esporta <cartella-chiusa> --scrivi <cartella-nuova>` (uscita in chiaro,
  scritta nel registro), si correggono i file, poi `kzero richiudi <cartella-in-chiaro> --scrivi <cartella-chiusa>`
  rimette i dati nella stessa cassaforte (stessi utenti, stesso registro) e la cartella in chiaro si cancella. Servono i
  permessi di tutte e due le classi. Domani lo faranno senza file in chiaro l'interfaccia del programma per Windows e il
  lettore del file del CTA, che scriveranno direttamente nella cassaforte.
- **Utenti**: `kzero aggiungi-utente`, `kzero togli-utente`, `kzero password` (la propria, o una nuova per un altro con
  `--per-utente`); **conservazione**: `kzero conservazione`; **cancellazione**: `kzero cancella`; **vedere**:
  `kzero cassaforte` (utenti, permessi, conteggi) e `kzero registro`.
- **Ogni comando che scrive o cambia qualcosa lo dichiara con `--scrivi <cartella>`**: la cartella nuova di una chiusura
  o di un'uscita in chiaro, oppure la cartella del pacchetto chiuso che cambia (utenti, password, conservazione, dati,
  cancellazione). Senza `--scrivi` un comando non cambia niente, tranne la voce del registro.

Esempio (password dall'ingresso standard, come nelle prove):

```
$ printf 'una frase lunga come password\n' | kzero chiudi chiaro --scrivi chiuso --utente TITOLARE --conservazione 2027-12-31 --password-da-ingresso
Pacchetto DEMO-2026 chiuso nella cartella nuova: file normali senza dati personali, cassaforte cifrata e registro degli accessi; riaperto e controllato, i dati sono identici all'originale. L'utente TITOLARE ha i dati personali e particolari.
Cassaforte 3f0c... del pacchetto DEMO-2026, chiusura numero 1; conservazione fino al 2027-12-31.
Valori: 248 personali e 37 particolari (mai mostrati qui).
Utenti: TITOLARE (dati personali e particolari).
Registro degli accessi: 1 voce.
Avviso: La cartella in chiaro 'chiaro' non e' stata toccata: va cancellata a mano. ...
```

## Cosa va nella cassaforte

Una regola sola, ricavata dalle marcature degli schemi (`x-kzero-classe`, src/api/classi.cpp):

- un campo personale o particolare va nella cassaforte intero, con tutto quello che contiene della stessa classe (es. il
  nome di un passeggero, la casa di una persona, l'elenco delle abilitazioni);
- un elemento di un elenco marcato tutto personale o particolare (oggi le assenze) va nella cassaforte intero, anche con
  il codice della persona: che l'elemento ci sia e' gia' un dato personale; nel file normale l'elenco resta vuoto;
- un oggetto rimasto vuoto perche' tutti i suoi campi sono andati nella cassaforte si toglie (non dice che c'era
  qualcosa); all'apertura si ricrea;
- i dati personali di un passeggero o di una casa legati a un luogo sensibile (formato-dati.md, "Luoghi sensibili"):
  nome, casa, indirizzo, coordinate. Sono dati particolari e vanno in una **sezione a parte, quella dei legati**, cifrata
  con una chiave ricavata dalla chiave dei personali **e** da quella dei particolari insieme: li decifra solo chi ha tutti
  e due i permessi. Chi ne ha uno solo non li decifra, nemmeno lavorando sul file con le chiavi che si procura (aggiunta
  1 del K-010; la prova interna lo verifica sul file, non solo sulle uscite);
- (K-012) il nome e la nota di un **committente privato** (una persona, es. chi prenota un noleggio per un matrimonio; il
  codice resta nel file normale): sono dati personali, nella sezione dei personali; se un servizio di un suo contratto va in
  un luogo sensibile (gruppi, fermate, passeggeri, una corsa che ci passa anche con i trasbordi o, con il porta a porta
  sensibile, case porta a porta) stanno nella **sezione dei legati**, come nome e casa dei passeggeri. La regola e' una sola
  (`LegameSensibile`, formato-dati.md). Il nome **non si perde mai**: il titolare e i delegati con i permessi lo vedono
  sempre, nome e cognome (se il servizio va in un luogo sensibile servono i permessi dei dati personali e particolari
  insieme); gli altri vedono il codice del committente. Nel demo chiuso: chi ha tutti e due i permessi vede CP-01 (il
  matrimonio) e CP-02 (il noleggio verso il centro diurno); chi ha solo i personali vede CP-01 e di CP-02 solo il codice; chi
  ha solo i particolari vede i codici;
- per la stessa ragione **un campo personale dentro un dato particolare** va tra i legati (regola ricavata dagli schemi, K-011
  aggiunta 2): oggi motivo e chi della dichiarazione del modo delle corse (dato particolare). Chi ha solo i particolari
  riceve la dichiarazione senza motivo e chi; chi ha solo i personali non la riceve. Un elemento marcato intero non puo'
  contenere campi dell'altra classe riservata (una prova lo controlla sugli schemi).

Il numero dei valori di ogni classe (contati come li conta `kzero controlla`) si scrive nell'intestazione: lo vedono
anche gli utenti che non possono aprire quella classe, mai i valori.

**Il legame tra un valore e il suo elemento.** Ogni valore e' legato al suo elemento con il **codice stabile**
dell'elemento, mai con la posizione in un elenco: riordinare un elenco nei file normali non cambia nulla. Un elemento
senza codice si riconosce dalla **chiave** dichiarata nel suo schema (`x-kzero-chiave`): turno e data per
un'assegnazione del giorno, il luogo per una fermata di un gruppo; solo se lo schema non dichiara nulla (oggi nessun
caso), dall'impronta dei suoi dati comuni e dall'ordine tra elementi identici (scambiarli non cambia niente di quello che
si vede). Un codice o una chiave ripetuti non prendono un numero d'ordine: l'unione li trova ambigui. Un valore che non
trova il suo elemento (tolto o cambiato dopo la chiusura) o ne trova piu' d'uno e' un errore **K-CTR-094**; un valore
scritto sia nella cassaforte sia in chiaro e' **K-CTR-095**: in tutti e due i casi il file non si usa. Mai un valore
sull'elemento sbagliato.

**La forma si controlla sui dati uniti.** Per chi non ha un permesso, i campi obbligatori di quella classe valgono come
presenti ma non leggibili (un dato personale di un passeggero puo' stare nella sezione dei legati: senza tutti e due i
permessi non si sa quale manca davvero), mai come mancanti.

**Altri controlli** (docs/controlli.md): cassaforte dichiarata nel manifesto e mancante (K-CTR-091), presente e non
dichiarata (K-CTR-092), cassaforte e file normali di chiusure diverse (K-CTR-093), dati personali o particolari in chiaro
senza "dati inventati" o accanto alla cassaforte (K-CTR-096, avviso), data di conservazione passata (K-CTR-097, avviso),
password da cambiare (K-CTR-098 ... K-CTR-100, avvisi: sezione "Utenti, permessi e chiavi").

## Utenti, permessi e chiavi

- **Tre chiavi di livello**, casuali, di 32 byte: **base** (la hanno tutti: autentica l'intestazione e cifra il
  registro), **personali** (cifra la sezione dei dati personali), **particolari** (cifra la sezione dei dati
  particolari). Ognuna ha una versione, che cresce quando la chiave si rifa'. La **sezione dei legati** (dati personali
  di chi va in un luogo sensibile) e' cifrata con una chiave ricavata da quelle dei personali e dei particolari insieme
  (HKDF-SHA256 di libsodium): serve averle tutte e due, nessuna delle due da sola basta.
- **Una fessura per utente**: la sua chiave segreta X25519 e le chiavi dei soli livelli che ha, cifrate
  (XChaCha20-Poly1305) con la chiave ricavata dalla sua password (Argon2id). I dati associati della fessura sono codice,
  permessi, chiave pubblica, sale e parametri dell'utente: cambiarne uno nell'intestazione e la fessura non si apre piu'.
- **Togliere un utente** rifa' la chiave base e le chiavi dei livelli che aveva (versione + 1), ricifra le sezioni di
  quei livelli e sempre quella dei legati, e consegna le chiavi nuove agli utenti rimasti che hanno quei livelli in una **busta** (`crypto_box_seal` per la loro
  chiave pubblica), senza conoscere le loro password. Le chiavi vecchie vanno nella **storia** del livello, cifrata con la
  chiave nuova. Chi apre e ha nella fessura una chiave vecchia prende quella nuova dalla sua busta, e la accetta solo se la
  storia contiene la chiave che ha gia': la busta l'ha preparata qualcuno che aveva la chiave vecchia, non chiunque
  conosca la sua chiave pubblica (che e' in chiaro). Una busta per un livello che l'utente non ha nella sua fessura non
  vale mai: i permessi non si alzano con le buste. Chi toglie un utente deve avere i dati personali **e** particolari,
  anche per togliere chi ne aveva uno solo: la sezione dei legati si ricifra con tutte e due le chiavi (DECISIONI.md,
  aggiunta 1 del K-010). Non si toglie l'ultimo utente, l'ultimo che legge i dati personali o particolari, ne' l'ultimo
  con tutti e due i permessi.
- **Aggiungere un utente**: chi lo aggiunge sceglie i permessi, al massimo i propri; il nuovo utente scrive la sua
  password e riceve una fessura con le chiavi di adesso dei suoi livelli.
- **Password nuova**: per se', oppure per un altro utente che l'ha persa (chi gliela da' deve avere tutti i suoi
  permessi). In tutti e due i casi la fessura si rifa' con le chiavi di adesso e una **coppia X25519 nuova**, e le buste
  di quell'utente non servono piu': chi conosceva la password vecchia (e con quella poteva aprire la fessura e leggere la
  chiave segreta di allora) non apre le buste dei prossimi cambi delle chiavi (aggiunta 1 del K-010). Non esiste un
  recupero ne' una chiave di ripristino.
- **Avvisi sulle password** (aggiunta 1 del K-010; testi nel catalogo dei controlli, docs/controlli.md; uguali in testo e
  in JSON): dopo `kzero aggiungi-utente` e `kzero password --per-utente` la password l'ha scritta un'altra persona, che
  la conosce: va cambiata al primo uso (**K-CTR-100**). Dopo `kzero togli-utente`: se l'utente tolto conosceva la
  password di un collega, con quella aprirebbe la fessura del collega e poi la sua busta con le chiavi nuove; chi l'ha
  tolto cambia subito la sua (**K-CTR-099**), e a chi ha ricevuto le chiavi nuove in una busta **ogni apertura** lo
  ricorda, in ogni comando, finche' non cambia la password (**K-CTR-098**).
- **Intestazione autenticata** (crypto_auth, HMAC-SHA-512-256): con una chiave ricavata dalla chiave base (solo chi apre
  la cassaforte la conosce, e si rifa' quando si toglie un utente) tutta l'intestazione; con una chiave ricavata dalla
  chiave dei personali l'elenco di chi ha i dati personali (codici e chiavi pubbliche); con una chiave ricavata dalla
  chiave dei particolari l'elenco di chi ha i dati particolari. Cosi' un utente con i soli dati personali, che conosce
  le chiavi base e dei personali, non puo' dare a se' o ad altri il permesso dei particolari: chi ha i particolari se ne
  accorge prima di qualunque uso, e nessun cambio di chiavi consegna la chiave dei particolari a chi non e' nell'elenco
  autenticato. Una firma non basterebbe: la sua chiave pubblica starebbe nello stesso file.
- **All'apertura**, prima di usare qualunque dato: forma del file, codice della cassaforte uguale a quello del manifesto,
  utente, parametri di Argon2id (rifiutati prima di calcolare se sono sotto il minimo o assurdi), fessura, buste e
  storie, autenticazione dell'intestazione e degli elenchi dei livelli dell'utente, impronte delle sezioni, sezioni dei
  suoi permessi. Solo dopo si consegnano chiavi o si mostrano dati.

### Parametri di Argon2id

Argon2id v1.3 (`crypto_pwhash`, un solo filo di calcolo come in libsodium). Fessure nuove: **256 MiB di memoria e 3
passaggi**; si accettano fessure da quei valori fino a 2 GiB e 10 passaggi; sotto, la fessura e' rifiutata (parametri
abbassati da fuori, per indovinare piu' in fretta); sopra, rifiutata prima di calcolare (parametri assurdi, per bloccare
il PC). Motivi e tempi misurati in DECISIONI.md. Solo le prove interne e il fuzzing, che sono programmi a parte, usano
parametri leggeri; il programma kzero non ha nessun parametro o variabile per cambiarli.

## Il registro degli accessi

- **Una voce** per ogni apertura e ogni azione: numero, data e ora UTC (dall'orologio del PC), utente, comando, classi
  aperte (decifrate) e classi mostrate nell'uscita, una nota senza dati personali (es. "giornata del 2026-10-22, mezzo
  ALFA-03", "aggiunto l'utente B con i dati personali"). Mai i dati. Anche chiusure, uscite in chiaro, utenti aggiunti e
  tolti, password, conservazione.
- **Prima la voce, poi i dati**: se la voce non si scrive, i dati non si aprono (K-ERR-030).
- **A catena**: ogni voce contiene l'impronta BLAKE2b-256 di quella prima (la prima, quella del codice della cassaforte).
  In piu' la cassaforte, ogni volta che si riscrive, ricorda numero e impronta dell'ultima voce (**ancora**): un
  registro accorciato o cambiato prima di quella voce si scopre (K-ERR-029).
- **Cifrato** con una chiave ricavata dalla chiave base: lo scrive e lo legge ogni utente, anche con un solo permesso
  (`kzero registro`, che e' a sua volta una voce). Dopo un cambio delle chiavi si ricifra con la chiave nuova.
- **Accanto alla cassaforte**, non dentro: un comando che legge non riscrive mai la cassaforte, solo il registro.
- **Durata**: quella della cassaforte; `kzero cancella` lo cancella con lei e stampa un resoconto da conservare.
- **Scritture**: sempre atomiche (file temporaneo nella stessa cartella, scritto e portato sul disco, poi sostituito con
  una sola operazione) e con la cartella bloccata (flock): due comandi insieme non perdono voci e non rovinano i file.
- **Ordine dei cambiamenti** (utenti, password, conservazione, dati): con la cartella bloccata, apertura e controlli,
  cambiamento in memoria, voce del registro (cifrata con la chiave base di prima, che la cassaforte nuova conserva nella
  storia), cassaforte nuova con l'ancora, e infine, se la chiave base e' cambiata, il registro ricifrato. Un'interruzione
  in qualunque punto lascia file interi che si aprono; al piu' una voce che descrive un cambiamento non arrivato in fondo.
  Una richiusura interrotta dopo la cassaforte e prima dei file normali si vede (K-CTR-093) e si ripara rifacendola.

### Cosa non si scopre

- Chi ha le chiavi (ogni utente) e sa programmare puo' riscrivere tutto il registro; le voci dopo l'ultima modifica della
  cassaforte (le sole letture) si possono togliere senza che si veda.
- Una copia vecchia di tutta la cartella (cassaforte, registro e file normali) rimessa al posto di quella nuova: i file
  sono autentici, solo vecchi. Le copie vecchie si aprono con le password e gli utenti di allora, anche quelli tolti dopo.
- Un utente tolto che aveva ancora accesso alla cartella e tutti i permessi del livello che si rifa' potrebbe preparare
  una versione finta delle chiavi, con una storia che contiene le chiavi vecchie che conosceva: chi non ha piu' aperto
  la cassaforte con una password nuova dopo la sua uscita la accetterebbe. Chi ha tolto l'utente (o ha cambiato password
  dopo) ha nella fessura la chiave nuova vera e se ne accorge (K-ERR-028). Rimedio: dopo aver tolto un utente, gli altri
  cambiano la password; il programma lo ricorda a ogni apertura finche' non lo fanno (K-CTR-098).
- L'ora e' quella dell'orologio del PC.
- I tentativi con una password sbagliata (o su una cassaforte manomessa) non entrano nel registro: senza la password non
  c'e' la chiave per scrivere una voce, e chi prova le password su una copia del file non passa dal programma. Il registro
  dice chi ha aperto, non chi ha provato.

## Il formato del file della cassaforte (versione 1)

Tutti i numeri sono interi senza segno, **little-endian**. "testo8" = 1 byte di lunghezza (1-64) seguito dai byte del
testo (un codice K-Zero: lettere senza accenti, cifre, `-`, `_`, `.`). Il file e' l'intestazione seguita dalle tre
sezioni; la sua lunghezza deve essere esattamente quella che l'intestazione dichiara.

| Parte | Byte | Contenuto |
|---|---|---|
| magia | 8 | `KZ-CASSA` (ASCII) |
| versione del formato | 2 | `1` |
| codice della cassaforte | 16 | lo stesso del manifesto (`cassaforte.id`, 32 cifre esadecimali) |
| generazione | 4 | numero della chiusura (1 alla chiusura, +1 a ogni richiusura); lo stesso del manifesto |
| versioni delle chiavi | 3 x 4 | base, personali, particolari (da 1 a 4096) |
| conservazione | 10 | `AAAA-MM-GG` (ASCII) |
| ancora del registro | 4 + 32 | numero dell'ultima voce scritta con la cassaforte e la sua impronta (BLAKE2b-256) |
| valori | 8 + 8 | quanti valori personali e quanti particolari (contati con le classi) |
| utenti | 2 + ... | quanti (da 1 a 64), poi per ognuno: |
| - codice | testo8 | |
| - permessi | 1 | bit 0 = dati personali, bit 1 = dati particolari (almeno uno) |
| - chiave pubblica | 32 | X25519 |
| - sale | 16 | di Argon2id |
| - operazioni | 4 | passaggi di Argon2id |
| - memoria | 4 | memoria di Argon2id in KiB |
| - nonce | 24 | della fessura |
| - fessura | 2 + n | lunghezza e testo cifrato (XChaCha20-Poly1305, etichetta di 16 byte compresa); n = 32 + 1 + 37 x (livelli) + 16 |
| buste | 2 + ... | quante (al massimo 192), poi per ognuna: |
| - codice dell'utente | testo8 | |
| - livello | 1 | 0 base, 1 personali, 2 particolari |
| - versione | 4 | della chiave dentro (almeno 2) |
| - busta | 101 | `crypto_box_seal` (48 byte in piu') di: codice della cassaforte (16), livello (1), versione (4), chiave (32) |
| storie | 3 x (4 + n) | per base, personali, particolari: lunghezza (0 con la versione 1), poi nonce (24) e testo cifrato delle chiavi vecchie: per ogni versione da 1 a quella di adesso meno uno, versione (4) e chiave (32) |
| sezioni | 3 x (24 + 8 + 32) | per personali, particolari e legati: nonce, lunghezza del testo cifrato, impronta BLAKE2b-256 del testo cifrato |
| autenticazione dei personali | 32 | crypto_auth dell'elenco di chi ha i dati personali |
| autenticazione dei particolari | 32 | crypto_auth dell'elenco di chi ha i dati particolari |
| autenticazione dell'intestazione | 32 | crypto_auth di tutti i byte dell'intestazione qui sopra |
| sezione dei dati personali | n | testo cifrato |
| sezione dei dati particolari | n | testo cifrato |
| sezione dei legati | n | testo cifrato (dati personali di chi va in un luogo sensibile) |

**Fessura** (testo in chiaro): chiave segreta X25519 (32), numero dei livelli (1), poi per ogni livello dell'utente in
ordine base, personali, particolari: livello (1), versione (4), chiave (32). **Dati associati**: magia, versione del
formato, codice della cassaforte, codice dell'utente (testo8), permessi (1), chiave pubblica, sale, operazioni, memoria.
**Chiave**: Argon2id della password normalizzata in NFC, con il sale e i parametri dell'utente (32 byte).

**Chiavi derivate** da una chiave di livello (`crypto_kdf`, contesto `KZ-CASSA`): numero 1 = cifratura delle sezioni,
2 = autenticazione (intestazione ed elenchi), 3 = storia, 4 = registro (solo dalla chiave base), 5 = sezione dei legati
(dalle chiavi dei personali e dei particolari). Ogni chiave ha un solo uso.

**Sezioni**: XChaCha20-Poly1305 con la chiave derivata 1 del livello, nonce casuale; dati associati: magia, versione del
formato, codice della cassaforte, livello (1 o 2), versione della chiave, generazione. Il testo in chiaro e' un elenco
JSON di voci, nell'ordine della separazione; ogni voce e' `{"file", "percorso", "campo", "valore"}` (un campo) oppure
`{"file", "percorso", "elemento"}` (un elemento intero, da aggiungere in fondo all'elenco del percorso). Il percorso e' un
elenco di passi dal documento al contenitore: un testo (un campo) oppure, in un elenco, `{"codice": "..."}`,
`{"chiave": {...}}` o `{"contenuto": "<impronta>", "n": <ordine>}`.

**Sezione dei legati**: XChaCha20-Poly1305, nonce casuale, stesso testo in chiaro delle altre sezioni. **Chiave**:
HKDF-SHA256 (RFC 5869, `crypto_kdf_hkdf_sha256_extract` e `crypto_kdf_hkdf_sha256_expand` di libsodium), con sale =
magia, versione del formato e codice della cassaforte; materiale = la chiave derivata 5 della chiave dei personali
seguita dalla chiave derivata 5 della chiave dei particolari (64 byte); contesto `KZ-CASSA dati legati`; 32 byte.
**Dati associati**: magia, versione del formato, codice della cassaforte, il byte 3, versione della chiave dei personali,
versione della chiave dei particolari, generazione. Si ricifra a ogni cambio di una delle due chiavi.

**Storie**: XChaCha20-Poly1305 con la chiave derivata 3 della chiave di adesso del livello; dati associati: magia,
versione del formato, codice della cassaforte, livello, versione di adesso.

**Elenco di un livello** (autenticato con la chiave derivata 2 di quel livello): `KZ-LISTA` (8), versione del formato
(2), codice della cassaforte (16), livello (1), versione della chiave (4), quanti utenti (2), poi per ogni utente con quel
livello, nell'ordine dell'intestazione: codice (testo8) e chiave pubblica (32).

Le prove interne (tests/test_cassaforte.cpp) costruiscono file cambiati seguendo proprio questa tabella, come farebbe un
altro programma; una copia della versione 1, fatta con il programma del K-010, resta nelle prove per sempre
(tests/esempi/k010-cassaforte, password di prova nelle prove).

## Il formato del file del registro (versione 1)

| Parte | Byte | Contenuto |
|---|---|---|
| magia | 8 | `KZ-REGIS` (ASCII) |
| versione del formato | 2 | `1` |
| codice della cassaforte | 16 | |
| versione della chiave base | 4 | con cui e' cifrato |
| nonce | 24 | |
| testo cifrato | resto | XChaCha20-Poly1305 con la chiave derivata 4 della chiave base; dati associati: i 30 byte prima del nonce |

Testo in chiaro: un elenco JSON di voci `{"numero", "ora", "utente", "comando", "classi_aperte", "classi_mostrate",
"nota", "precedente"}`; `precedente` e' l'impronta BLAKE2b-256 (64 cifre esadecimali) della voce prima, scritta come JSON
compatto con le chiavi in ordine alfabetico; per la prima voce, l'impronta di `KZ-REGIS` seguito dal codice della
cassaforte.

## Le uscite

I comandi della cassaforte rispondono con lo schema `cassaforte` (versione 1: [schema/cassaforte.v1.schema.json](
schema/cassaforte.v1.schema.json)); `kzero registro` con lo schema `registro` (versione 1:
[schema/registro.v1.schema.json](schema/registro.v1.schema.json)). `kzero controlla` (versione 4) e `kzero giornata`
(versione 2) hanno i campi `cassaforte` (chi l'ha aperta, con quali permessi, la data di conservazione) e
`non_verificato`. Nessuna uscita contiene mai i valori della cassaforte se non quelli che l'utente puo' aprire e ha
chiesto di vedere.

Orari di lavoro e tipo di servizio (K-013): quello che dipende dal tipo di servizio delle corse (i regimi che ne dipendono,
il lavoro che contano, il lavoro per il contratto, i riposi secondo quei regimi, i loro problemi) lo vedono allo stesso modo
il titolare senza `--includi-dati-particolari` e l'utente con i soli dati personali: per tutti e due non c'e'. Cambia la
parola: per il titolare e' **"riservato"** (i dati li ha e il programma li ha calcolati: li vede con il parametro, e i problemi
contano nel riepilogo), per l'altro **"non verificato"** (non puo' aprire il tipo di servizio, quindi il programma non li
calcola). In `kzero ore` la riga fissa della persona ("Regimi che dipendono dal tipo di servizio delle corse: riservati" oppure
"non verificati"; in JSON `regimi_dal_tipo_di_servizio.esito`) e, dal K-013, anche quella dei regimi seguiti per scelta
dell'azienda dice "non verificati" a chi non ha il permesso. Il resto degli orari (i regimi che non dipendono dal tipo, i
rientri) resta come prima: il titolare lo vede, per l'utente con i soli dati personali resta "non verificato", perche' anche il
suo piano dei turni lo e' (gli usi dei mezzi dipendono dal tipo di servizio). Decisione chiesta prima di scrivere il codice:
DECISIONI.md, K-013.

Gli avvisi della cassaforte (K-CTR-097 data di conservazione passata, K-CTR-098 ... K-CTR-100 password da cambiare) hanno
i testi del catalogo dei controlli, uguali in testo ("Avviso K-CTR-nnn: ...") e in JSON (`codice` e `messaggio`): in
`kzero controlla` stanno tra i problemi; in `kzero giornata` nel campo `cassaforte.avvisi`; in `kzero registro` e nei
comandi della cassaforte nel campo `avvisi`, dove le indicazioni del comando (es. la cartella in chiaro da cancellare)
hanno `codice` null.
