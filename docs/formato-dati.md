# Formato dei dati di K-Zero

Questo documento spiega come si scrivono i dati che K-Zero legge. Parte 1 (K-007): luoghi e fermate,
regolamenti, calendari, scuole e centri, committenti e contratti, servizi con la loro domanda, corse, mezzi e
rimesse. Parte 2 (K-008): elenchi e impostazioni delle aziende, norme del paese, festivita', persone, assenze,
fermi, turni del giorno, rotazioni, servizi fissi, assegnazioni del singolo giorno, blocchi.

Il formato vale per qualunque azienda, consorzio o ente, di qualunque dimensione e di qualunque paese:
nel programma non c'e' nulla di specifico di un'azienda o di un territorio. Le regole di un territorio
stanno in un **regolamento**, che e' un dato; le parole di un'azienda (es. "primo", "lotto 2") sono solo
nomi nei dati.

Per controllare un pacchetto: `kzero controlla <cartella>` (problemi spiegati in [controlli.md](controlli.md)).
Esempi completi: `esempi/demo` (livello "completo") ed `esempi/demo-numeri` (livello "solo numeri"),
con dati inventati. Dal K-008 il demo ha anche persone e turni: in Alfa la rotazione di 14 giorni della forma di
quella di ALP, un servizio fisso solo il mercoledi', sostituzioni, un mezzo in revisione sostituito dal mezzo di
scorta, un autobus di linea con la distrazione al noleggio usato per una gita, il cambio dell'autista a meta' corsa,
turni notturni, spezzati, di riserva e di reperibilita'; in Beta una linea 7 su 7 con orario feriale e festivo e un
ciclo di 3 giorni. Dal K-012 la gita della Pro Loco e' un servizio NCC e ci sono due committenti privati: il matrimonio di
CP-01 (sabato 2027-07-10) e il noleggio di CP-02 verso il centro diurno (mercoledi' 2027-07-07). La giornata di un giorno si
vede con `kzero giornata esempi/demo --data 2026-10-22`.

## Il pacchetto dati

Un **pacchetto** e' una cartella con un **manifesto** (`pacchetto.json`) e un file JSON per ogni tipo di dato.
Solo il manifesto e' obbligatorio; un file che manca vuol dire "nessun elemento di quel tipo".

| File | Obbligatorio | Contenuto |
|---|---|---|
| `pacchetto.json` | si' | Manifesto: aziende, periodo, origine, livello, fuso orario, valuta, impostazioni |
| `aziende.json` | no | Elenchi e impostazioni di ogni azienda: tipi di attivita', di assenza, di fermo, competenze, norme, ricovero, abbinamento autista-mezzo |
| `norme.json` | no | Norme di un paese: abilitazioni per guidare, usi dei mezzi, requisiti di guida, usi per tipo di servizio, con la fonte |
| `luoghi.json` | no | Luoghi pubblici (fermate, scuole, centri, rimesse...) e luoghi privati (case) |
| `tratti.json` | no | Tratti di strada tra due luoghi con i loro limiti |
| `regolamenti.json` | no | Regolamenti: i parametri di un territorio o di un committente, con la fonte |
| `festivita.json` | no | Calendari delle festivita' di un paese o di un territorio, con la fonte |
| `calendari.json` | no | Calendari: giorni della settimana, date aggiunte e tolte, sospensioni, altri calendari e festivita' aggiunti o tolti |
| `scuole.json` | no | Scuole e centri: grado o tipo di centro, orari, attese concordate |
| `committenti.json` | no | Committenti |
| `contratti.json` | no | Contratti: committente, azienda, regolamento, regole del committente sul mezzo |
| `gruppi_servizi.json` | no | Gruppi di servizi affidati insieme (es. un "lotto") |
| `servizi.json` | no | Servizi con la loro domanda (gruppi di passeggeri) |
| `corse.json` | no | Corse con i passaggi, i trasbordi, le coincidenze, i km |
| `mezzi.json` | no | Mezzi: posti, attrezzature, costi, uso della carta di circolazione, distrazioni, vincolo di destinazione, scorta, prestiti |
| `rimesse.json` | no | Rimesse e ricoveri dei mezzi |
| `tempi_sosta.json` | no | Tempi di sosta alle fermate (facoltativi finche' non confermati) |
| `persone.json` | no | Persone delle aziende: autisti e assistenti, ruoli, rapporto, ore, disponibilita', abilitazioni, competenze, preferenze |
| `assenze.json` | no | Assenze programmate delle persone: periodo e tipo, mai la causa |
| `fermi.json` | no | Fermi programmati dei mezzi (es. revisione) |
| `turni.json` | no | Turni del giorno: corse intere o a pezzi e attivita' in ordine, mezzo, luoghi di inizio e fine, calendario |
| `rotazioni.json` | no | Rotazioni: cicli di passi (giorni o mesi) con i turni di ogni persona |
| `servizi_fissi.json` | no | Turni assegnati sempre alla stessa persona nei giorni di un calendario |
| `assegnazioni.json` | no | Assegnazioni del singolo giorno: sostituzioni, scambi, un mezzo al posto di un altro, lavori occasionali |
| `blocchi.json` | no | Blocchi: corse fissate su un mezzo, per il calcolo futuro |

Un file con un nome diverso da questi non viene letto (avviso K-CTR-008): di solito e' un nome scritto male.

### Regole comuni a tutti i file

- **Codifica UTF-8.** Ogni file si salva in UTF-8, la codifica dei file JSON (con o senza il segno iniziale
  che aggiungono alcuni programmi di Windows). Un file salvato come "ANSI" o come "Unicode" (UTF-16) dal Blocco
  note o da un programma vecchio non viene letto (errore K-CTR-048, con riga e colonna del primo carattere
  sbagliato): basta salvarlo di nuovo scegliendo UTF-8.
- **Schema e versione.** Ogni file comincia con `"schema"` (es. `"luoghi"`) e `"schema_versione"` (oggi 1).
  La forma di ogni file e' descritta da uno schema JSON (draft-07) in `docs/schema/<schema>.v<versione>.schema.json`:
  e' l'unica fonte delle regole di forma, e il programma controlla i file proprio con quegli schemi
  (incorporati nel programma). Una versione che il programma non conosce = file rifiutato con il codice
  K-CTR-006, mai letto a meta'. Se la forma di un file cambia, cambia la versione.
- **Un file con un errore di forma non viene usato per niente**, e i controlli sul contenuto non si fanno
  finche' la forma non e' a posto (darebbero risultati falsati).
- **Codici.** Ogni elemento ha un codice K-Zero scelto da chi scrive i dati, stabile, unico per tipo di
  elemento (due fermate non possono avere lo stesso codice; una fermata e una corsa si'). Lettere senza
  accenti, cifre, `-`, `_` e `.`, al massimo 64 caratteri; inizia con una lettera o una cifra. I codici
  dei gruppi di passeggeri e dei passeggeri sono unici in tutto il pacchetto.
- **Codici esterni.** Luoghi, calendari, scuole, committenti, contratti, servizi, corse e mezzi possono avere
  un elenco `codici_esterni`: lo stesso elemento in un altro sistema, scritto come **sistema + tipo + codice**
  (es. sistema "Trasporti Provinciali", tipo "fermata", codice "6220PF"; sistema "Consorzio", tipo "servizio",
  codice "1234567/1"). Cosi' il formato vale per qualunque committente senza campi dedicati.
- **Orari** `"HH:MM"`, ora locale del fuso del pacchetto. Dopo mezzanotte si continua a contare, come in GTFS:
  una corsa che parte alle 23:40 e arriva alle 00:12 del giorno dopo si scrive `"23:40"` ... `"24:12"`.
  Ammessi fino a `"47:59"`. Anche nelle uscite del programma un orario oltre le 24:00 resta tale.
- **Date** `"AAAA-MM-GG"`, solo date che esistono (il 29 febbraio solo negli anni bisestili).
- **Coordinate** WGS84 in gradi decimali: `{"lat": 46.4421, "lon": 11.0712}`.
- **Distanze** in km (nei regolamenti, dove il nome del campo lo dice, in metri: `_m`); **misure** in metri,
  **masse** in tonnellate; **costi** nella valuta del manifesto.
- **Testi**: una riga non vuota fino a 300 caratteri; le note (`nota`) fino a 2000 caratteri.
- **Dimensione**: un file piu' grande di 256 MB non viene letto (errore K-CTR-053): e' certamente un errore, e
  leggerlo potrebbe esaurire la memoria del PC. La lettura si ferma appena passa il massimo, anche quando il sistema
  non dice la dimensione o il file cresce mentre lo si legge.
- **File normali dentro la cartella**: ogni file del pacchetto e' un file normale. Un dispositivo (es. `/dev/zero`),
  una "pipe" o una cartella con il nome di un file del pacchetto non vengono letti (errori K-CTR-088 e K-CTR-090). Un
  collegamento (link simbolico) si segue solo se porta a un file normale dentro la cartella del pacchetto, anche in una
  sottocartella (altrimenti errore K-CTR-089); la cartella del pacchetto puo' essere a sua volta un collegamento (es.
  una cartella sincronizzata).
- **Numeri**: ogni numero ha un minimo e un massimo di buon senso (es. 1.000 posti per mezzo, 10.000 persone a
  una fermata, 1.440 minuti, 3.600 secondi, 100 m per una misura, 1.000.000 per i valori di un regolamento):
  un valore piu' grande e' certamente un errore di scrittura e il file viene rifiutato (K-CTR-007).
- **Elenchi aperti.** Dove un valore dipende dal paese o dal committente (tipo di servizio, tipo di gruppo,
  tipo di centro, alimentazione...) c'e' il valore `"altro"` con una descrizione. Le categorie dei mezzi
  M1, M2, M3 sono europee e restano fisse.
- **Campi non previsti** dallo schema sono un errore (K-CTR-007): cosi' un campo scritto male non viene
  ignorato in silenzio.

### Fuso orario, ora legale, valuta

Il manifesto dichiara il **fuso orario** (nome IANA, es. `"Europe/Rome"`) e la **valuta** (codice ISO 4217,
es. `"EUR"`). Tutti gli orari del pacchetto sono **orari locali**, cioe' l'ora dell'orologio in quel fuso,
come su un orario stampato. K-Zero non converte mai gli orari e non usa l'orologio del computer.

Giorni del cambio dell'ora legale (in Europa l'ultima domenica di marzo e di ottobre): l'orario resta
quello scritto e una durata si calcola come differenza tra due orari scritti. Una corsa che attraversa
l'ora del cambio (tra le 02:00 e le 03:00) dura quindi nella realta' un'ora in piu' o in meno di quanto
risulta dai dati. E' un limite noto e voluto: i servizi di questo formato quasi mai viaggiano in quell'ora,
e le corse notturne di quei due giorni vanno controllate a mano. Dal K-011 le date del cambio sono un dato del paese,
con la fonte (`norme.json`, `cambi_ora`; in Europa la direttiva 2000/84/CE), mai il fuso orario del computer: un turno
che attraversa l'ora del cambio ha l'avviso K-CTR-103 "da controllare a mano".

### Classi dei dati

Ogni campo e' marcato nello schema con la sua **classe** (`"x-kzero-classe"`). Il filtro delle uscite
parte da quella marcatura ed e' uguale per testo e JSON.

| Classe | Cosa comprende | Nelle uscite, di base | Per vederli |
|---|---|---|---|
| comune | fermate pubbliche (nome, posizione, codici), scuole e centri, grado, orari, calendari, conteggi senza esigenze, mezzi (targa compresa), aziende, committenti, contratti, regolamenti, tutti i codici | sempre visibili | - |
| personale | nomi e cognomi, indirizzi, case, punti porta a porta (sono la casa del passeggero), ricoveri presso la casa di una persona, motivi scritti a mano; delle persone (autisti e assistenti): ruoli, nome, casa, rapporto di lavoro, contratto collettivo, ore, disponibilita', costo orario, abilitazioni, autorizzazioni, competenze, preferenze, note; le assenze; le note dei file che parlano di persone | sostituiti dal **codice** dell'elemento (o da `"riservato"`, se non c'e' un codice da mostrare) | `--includi-dati-personali` |
| particolare | dati sulla salute: esigenze di una persona o di un gruppo (carrozzina, ausili piegati, assistente necessario), tipo di servizio e di gruppo ("disabili"), conteggi di carrozzine e di ausili piegati, pedana obbligatoria per un servizio, competenze richieste da un servizio o da un turno, categoria ricavata della corsa | sostituiti da `"riservato"` | `--includi-dati-particolari` |

Perche' i dati particolari sono nascosti anche quando al posto del nome c'e' un codice: sono dati sulla
salute, e restano dati personali anche senza nome (GDPR, considerando 26); in un paese piccolo
"1 carrozzina alla fermata X" basta a riconoscere una persona. L'uscita grezza del programma spesso la
legge un assistente AI su server esterni: per questo di base non li mostra. L'interfaccia per persone
(futura) li mostrera' a chi pianifica.

Un campo senza marcatura vale, per prudenza, come particolare: una prova controlla che tutti i campi
abbiano la loro classe, che i dati personali stiano solo nei luoghi privati e nei passeggeri, e quali sono
i dati particolari.

**Cassaforte** (K-010, [cassaforte.md](cassaforte.md)). In un pacchetto chiuso tutti i valori personali e particolari,
scelti con queste stesse classi, stanno nella cassaforte cifrata (`cassaforte.kzc`); i file normali hanno solo i dati
comuni e, da soli, non rispettano lo schema (mancano i campi obbligatori personali): lo schema si controlla sui dati
uniti. Ogni valore e' legato al suo elemento con il codice; un elemento senza codice con la **chiave** dichiarata nello
schema (`"x-kzero-chiave"`: turno e data di un'assegnazione del giorno, luogo di una fermata di un gruppo). Un elemento
che ha valori nella cassaforte non va tolto ne' cambiato nei file normali: si corregge con un'uscita in chiaro e una
richiusura (`kzero esporta`, `kzero richiudi`). Un pacchetto senza cassaforte con dati personali o particolari e senza
`"dati_inventati": true` ha l'avviso K-CTR-096 a ogni lettura: i dati veri stanno solo nella cassaforte.

Dal K-009 le stesse classi decidono anche le **posizioni** dei problemi (di base non entrano mai in un dato personale
o particolare), quali problemi si contano soltanto (quelli sui dati sulla salute) e quanti dati personali e
particolari ci sono in un pacchetto: docs/controlli.md e docs/sicurezza.md. Una marcatura puo' stare anche su un elemento
intero o su un file intero (es. un'assenza): vale per tutto quello che contiene, tranne i campi marcati diversamente
(es. il codice della persona). Un luogo pubblico puo' essere **sensibile** (luoghi.json, versione 2).

### Due livelli di pacchetto

Il manifesto dichiara il **livello**:
- **`solo_numeri`** (consigliato): conteggi per fermata; nessun nome, indirizzo, punto porta a porta o
  motivo scritto. Niente luoghi privati e niente passeggeri uno per uno (errore K-CTR-019);
- **`completo`**: anche i passeggeri uno per uno e le loro case.

I conteggi di carrozzine e di ausili piegati (dati particolari aggregati) sono ammessi in entrambi i livelli,
sempre filtrati nelle uscite.

**Persone (autisti e assistenti) nei due livelli.** Anche un pacchetto "solo numeri" ha le persone: senza di loro
non si possono controllare ne' pianificare i turni. Restano i dati che servono al calcolo (codice, azienda, ruolo,
abilitazioni, competenze, disponibilita', ore, preferenze, mezzo abituale), che sono comunque nascosti nelle uscite
di base; non ci sono invece il nome, la casa e le note scritte a mano (errore K-CTR-019), che per controllare e
pianificare non servono. Le assenze restano: dicono solo quando una persona manca e il tipo, mai la causa.

**Minimizzazione:** il formato non ha campi per codice fiscale, telefoni, mail delle famiglie, data di
nascita; per le persone delle aziende nemmeno idoneita' medica e cause delle assenze (una malattia e' un dato sulla
salute e per pianificare non serve). K-Zero non li conserva mai. Un campo cosi' aggiunto a mano e' un errore
("campo non previsto").

### Orari di lavoro: categorie di tempo, regimi, tipi di regola (K-011)

I tempi di ogni persona si ricavano dai turni (chi fa il turno come in `kzero giornata`): la **giornata** della persona
sono tutti i turni che comincia in quel giorno di calendario, anche di aziende diverse (prestiti: la legge somma le ore,
D.Lgs. 234/2007 art. 4 c. 4); un turno oltre la mezzanotte resta del giorno in cui comincia. Il tempo tra due giornate e'
**riposo**.

**Categorie di tempo.** Ogni minuto della giornata ha una categoria, come nel Reg. (CE) 561/2006 (art. 4 e 16) e nel
D.Lgs. 234/2007 (art. 3): `guida`, `altre_mansioni`, `disponibilita` (sul posto: pronto a partire, attesa con il mezzo),
`reperibilita` (fuori dal posto di lavoro, a chiamata), `interruzione` (pausa, libero da ogni servizio). La categoria la
da':
- la **corsa**: guida per chi guida, disponibilita' per il secondo autista (D.Lgs. 234/2007 art. 3 b n. 2), altre
  mansioni per l'assistente a bordo;
- il **tipo di attivita'**, con il campo `tempo` dell'elenco dell'azienda (aziende.json versione 2); un'attivita' di guida
  (es. un trasferimento a vuoto) vale come la corsa per chi non guida;
- i **minuti non scritti** tra due elementi della giornata, con l'impostazione `soste_non_scritte` dell'azienda
  (obbligatoria nella versione 2): una categoria, e se serve un'altra oltre N minuti (es. fino a 30 minuti
  disponibilita', oltre interruzione).

Un file aziende.json della versione 1 si legge ancora: un tipo senza categoria conta per prudenza come altre mansioni (mai
come interruzione o riposo) e le soste non scritte come disponibilita'; le violazioni che vengono solo da questa prudenza
sono avvisi, e l'azienda ha un avviso con l'elenco dei tipi (K-CTR-101).

**Regimi.** Le leggi sugli orari e i contratti collettivi sono **dati** (norme.json versione 2, `regimi`): un regime e' un
insieme di regole di una fonte, con dove si applica. Il programma conosce solo i **tipi di regola** (come si conta); i
numeri e le fonti stanno nei dati. Un regime si applica a una persona in un giorno secondo:
- i **ruoli** del turno (`autista`, `secondo_autista`, `assistente`), i **rapporti** della persona e i suoi **contratti
  collettivi** (dati personali);
- le **corse** del giorno: tipi di servizio (dato particolare), mezzo (persone trasportabili compreso il conducente:
  posti + posti in piedi + 1; con i soli posti a sedere la condizione e' certa, solo con i posti in piedi e' da
  confermare), percorso della corsa (km dal primo all'ultimo passaggio, pieni e vuoti, oppure i `km_totali`) ed
  esclusioni (es. servizi regolari di linea fino a 50 km). Basta una corsa; ogni condizione puo' essere certa o da
  confermare. Senza km il regime e' "non determinabile" (avviso K-CTR-102) e si controlla per prudenza come se valesse;
  una giornata senza corse vale da confermare;
- l'**estensione**: una sola corsa che fa scattare il regime vale per tutta la settimana (lunedi' 00:00 - domenica 24:00)
  della persona, per alcune famiglie di regole (nota INL 61/2021: guida e riposi), e puo' **escludere** un altro regime
  per quelle famiglie (o lasciarlo solo da confermare). Un regime che prende le condizioni sulla corsa da un altro
  (`come_regime`, es. il D.Lgs. 234 dal Reg. 561) nei giorni in cui l'altro vale solo per l'estensione vale come dice
  `estensione_come_regime`: `da_confermare` (di base: che la segua non e' scritto nella legge, REGOLE.md) oppure `vale`.
  Con "da confermare" le sue violazioni in quei giorni sono avvisi con il motivo (K-011 aggiunta 1);
- le **esclusioni** delle corse: una voce esclude una corsa se ogni tipo di servizio della corsa e' tra i suoi tipi.
  Con `insieme_a` (K-011 aggiunta 1) la voce vale per i suoi tipi solo se la corsa ha anche uno di quei tipi (es. i
  disabili sulla stessa corsa dello scolastico: chi sale in piu' non cambia il tipo di servizio); le voci si sommano
  (es. "scolastico fino a 50 km" certa, "disabili insieme allo scolastico" certa, "disabili" da sola da confermare).

**Guida delle corse escluse** (`guida_delle_corse_escluse` del regime, K-011 aggiunta 1). Per le regole di guida e di
interruzione di un regime (guida di fila, guida massima) i minuti di guida delle corse che il regime esclude possono
contare come altre mansioni invece che come guida: nel Reg. 561/2006 la guida fuori dal suo campo e' "altre mansioni"
(art. 6 c. 5; guida TRACE II della Commissione; chiarimento n. 5 ad ANAV). `conta_come` e' la lettura certa,
`prudente` quella prudente (nel demo: altre mansioni nella certa, guida nella prudente, con un avviso); le corse escluse
solo "da confermare" nella lettura prudente sono sempre guida. Per il conteggio del lavoro restano lavoro. Un
trasferimento a vuoto segue la corsa a cui serve: quella dopo se arriva dove la corsa comincia, altrimenti quella prima se
parte da dove la corsa finisce; se non si capisce, conta da confermare (guida nella lettura prudente). Senza il campo la
guida e' guida per tutte le regole.

I regimi si **sommano**: una persona in un giorno puo' averne piu' di uno (es. il 561 per guida e riposi e il 234 per
l'orario di lavoro) e deve rispettarli tutti. Un giorno senza nessun regime non e' mai "regolare": `kzero ore` lo dice e,
per un dipendente, `kzero controlla` da' un avviso (K-CTR-107). I contratti collettivi sono regimi di tipo
`contratto_collettivo` nelle stesse norme, con la condizione `contratti_collettivi` (i codici scritti nelle persone):
stessi tipi di regola, una sola lettura, un solo posto da tenere aggiornato.

**Tipi di regola** (elenco chiuso, scritto nel programma; il "giorno" di ogni regola lo dice la regola, campo `periodo`):

| Tipo | Cosa conta | Campi (minuti, giorni) |
|---|---|---|
| `conteggio_lavoro` | Come il regime conta il lavoro: categorie contate, soste fino a N minuti contate per intero, quota del tempo inoperoso fuori residenza (fuori dal comune della presa servizio) o in residenza | `nome_misura`, `categorie`, `soste_fino_a_minuti`, `inoperoso_fuori_residenza`, `inoperoso_in_residenza` |
| `lavoro_massimo` | Lavoro nella giornata, nelle 24 ore dall'inizio, nella settimana, nel mese, o in media alla settimana su un periodo mobile; oltre il normale lo straordinario (avviso con l'obbligo scritto nei dati) | `periodo`, `massimo_minuti`, `normale_minuti`, `media`, `straordinario` |
| `guida_continua` | Guida massima prima di un'interruzione che azzera il conto (anche in parti nell'ordine, es. 15 + 30; alcune solo per i servizi occasionali); interruzioni brevi contate nella guida; eccezione per le soste ai capolinea | `massimo_minuti`, `azzera_con`, `interrompono`, `interruzioni_brevi_contate`, `eccezione_soste_capolinea_oltre_minuti` |
| `guida_massima` | Guida tra due riposi giornalieri (con l'estensione ammessa N volte a settimana), nella giornata, nella settimana, in due settimane, nel mese | `periodo`, `massimo_minuti`, `estensione` |
| `lavoro_continuo` | Lavoro di fila senza pausa e pause minime secondo il lavoro della giornata, in parti di almeno N minuti | `massimo_minuti`, `pause`, `frazione_minima_minuti` |
| `riposo_giornaliero` | Nelle 24 ore dall'inizio della giornata, entro 24 ore dalla fine del riposo precedente (regolare, frazionato nell'ordine, ridotto al massimo N volte tra due riposi settimanali), oppure tra due giornate; media su un periodo | `misura`, `minimo_minuti`, `frazionato`, `ridotto`, `media_riposo`, `reperibilita_interrompe` |
| `riposo_settimanale` | Riposo in ogni settimana (un riposo a cavallo di due settimane conta in una sola), ridotto, regolari in due settimane, entro N periodi di 24 ore, cumulo con la settimana accanto, compensazione entro N settimane attaccata a un riposo | `minimo_minuti`, `ridotto`, `regolari_in_due_settimane`, `entro_periodi_di_24_ore`, `cumulo_settimane`, `compensazione` |
| `lavoro_notturno` | Minuti di lavoro in una fascia (anche a cavallo della mezzanotte) e lavoro massimo nelle 24 ore con lavoro notturno | `fascia`, `massimo_minuti` |
| `giornate_massime` | Giornate di lavoro in un periodo mobile di anni, a ritroso dal giorno (con le giornate prima del pacchetto della persona) | `massimo_giornate`, `anni` |
| `ampiezza_massima` | Dalla prima all'ultima attivita' della giornata | `massimo_minuti` |
| `giorni_consecutivi` | Giorni di lavoro di fila | `massimo_giorni` |

**Letture certe e prudenti.** Una regola ha la sua lettura certa (i campi della regola) e, se la legge e' incerta, una
lettura **prudente** piu' severa (`prudente`: gli stessi campi, che cambiano solo dove serve, e la `nota` che spiega
perche'). Violare la lettura certa, con un regime che vale con certezza, e' un **errore**; violare solo la prudente, o
solo con un regime, un rapporto, un tipo di servizio, un mezzo o i km da confermare, un **avviso "da confermare"** con i
motivi. Una regola o un regime `da_confermare` senza lettura certa da' solo avvisi. Il programma non sceglie mai la
lettura piu' comoda per dire "regolare", e una finestra che esce dal periodo del pacchetto si conta con i dati che ci
sono: "verifica parziale" (K-CTR-104).

**Che cosa conta come "ora" per il contratto della persona** (`persone[].ore[]`): il lavoro contato dal regime che vale
quel giorno per la persona (es. il lavoro effettivo della L. 138, l'orario di lavoro del D.Lgs. 234); senza regime guida,
altre mansioni e disponibilita' sul posto. I conteggi possibili (K-011 aggiunta 1) sono quelli dei regimi che valgono con
certezza, quelli dei regimi da confermare e, se nessun regime vale con certezza, quello senza regime: nella lettura certa
conta il **piu' basso**, in quella prudente il **piu' alto**. Le ore `massime` sono limiti rigidi: errore K-CTR-119 solo
se tutti i conteggi superano il limite, altrimenti un avviso con il motivo (K-CTR-120). Le `previste` si mostrano in
`kzero ore` con la differenza. Le scelte sul lavoro massimo dell'azienda e della persona si contano allo stesso modo.

**Scelte dell'azienda e della persona** (aziende.json e persone.json versione 2, `scelte_orari`): facoltative, con la
forza `preferenza` (avviso) oppure `limite` (rigido: errore su un turno ricorrente, avviso su un'assegnazione del singolo
giorno, perche' la scelta dell'utente vince). Tipi: `lavoro_massimo` (giornata, settimana, mese), `guida_massima`
(giornata, settimana), `ampiezza_massima`, `riposo_giornaliero` (riposo minimo tra due giornate), `giorni_consecutivi`.
Una scelta della persona vince su quella dell'azienda dello stesso tipo e periodo. La legge resta sempre sopra: una
scelta piu' larga della legge si accetta con un avviso (K-CTR-106).

**Persona in prestito** (un turno di un'altra azienda, K-011 aggiunta 1). Le scelte sugli orari (`scelte_orari`) sono
quelle dell'azienda della persona (e le sue), anche nei turni che fa per un'altra azienda: sono un patto tra la persona e
chi la impiega. Le soste non scritte e la chiave del rientro sono invece quelle dell'azienda del turno: dicono come
quell'azienda organizza il suo turno. Le leggi si sommano su tutta la giornata, di qualunque azienda (D.Lgs. 234/2007 art.
4 c. 4). Le preferenze del K-008 (`fascia_oraria`,
`giorni_liberi`, `evita_turni`, `pausa_massima`) restano dove sono: le scelte nuove non le ripetono.

**Modo delle corse: OUT o tachigrafo** (aziende.json versione 2, `modo_corse`; decisione del supervisore del 28/10, K-011
aggiunta 1). Dove la legge lo permette (es. linee e scolastico fino a 50 km, fuori dal Reg. 561/2006) un'azienda puo'
lavorare **OUT** (con le regole nazionali, es. la L. 138) oppure registrare le corse con il **tachigrafo** e seguire per
scelta il regime (`regime`, es. R-561, e chi ne prende le condizioni con `come_regime`, es. il D.Lgs. 234). La scelta
si scrive per tipo di servizio (`per_tipo_servizio`), per servizio (`servizi`) o per corsa (`corse`): vince la piu'
precisa (corsa, poi servizio, poi tipo). Senza scelta decide la legge.
- **Tachigrafo**: il regime vale "per scelta dell'azienda" anche dove la legge lo esclude, con la sua `forza`
  (`preferenza`, di base: avvisi; `limite`: errore su un turno ricorrente, avviso su un'assegnazione del singolo giorno),
  anche con l'estensione alla settimana. Le violazioni hanno codici loro (K-CTR-127 e 128), separati da quelli della
  legge; dove vale la legge, vale la legge. La L. 138 resta sopra comunque (non la toglie una scelta).
- **OUT dove la legge vuole il regime** (es. una linea di 50,1 km): la scelta non vale, vale la legge, con un avviso
  (K-CTR-132) che suggerisce la forzatura se l'azienda e' sicura.
- **Scelte diverse sulla stessa corsa** (servizi o tipi diversi, il tachigrafo da una parte sola): decide l'impostazione
  `tipi_diversi` (`piu_prudente`, di base: il tachigrafo; oppure `out`, `tachigrafo`), con un avviso che suggerisce di
  scrivere la scelta per la corsa (K-CTR-133).
- **Dichiarazione** (`dichiarazione`, solo con `out`, per un servizio o una corsa): dove l'esclusione e' da confermare
  (es. un servizio solo per disabili che per l'azienda e' un servizio scolastico speciale), l'azienda lo dichiara
  escluso; da li' l'esclusione e' certa, con una sola nota (K-CTR-131) invece degli avvisi da confermare su ogni corsa.
  Se nel periodo del pacchetto non cambia niente (l'esclusione non e' da confermare, o nessun giorno con una persona), un
  avviso: K-CTR-135 (K-011 aggiunta 2).
- **Forzatura cosciente** (`forzatura`, solo con `out`, per un servizio o una corsa): dove con i dati scritti vale il
  regime (es. i km scritti sono sbagliati), l'azienda sceglie comunque OUT. Il programma pianifica come ha scelto
  l'azienda (nella lettura certa il regime non vale), ma la forzatura si vede sempre (`kzero controlla` K-CTR-129, il
  modo della corsa in `kzero giornata` e `kzero ore`) e il rischio e' un avviso: le violazioni che ci sarebbero con la
  legge come la calcolano i dati sono avvisi con il motivo "forzatura dell'azienda" e K-CTR-130 le riassume per corsa o
  servizio. Se nel periodo del pacchetto non cambia niente (con i dati scritti il regime non vale con certezza, o nessun
  giorno con una persona), un avviso: K-CTR-134 (K-011 aggiunta 2).
Dichiarazione e forzatura hanno `motivo` e `chi` (dati personali), `quando` e, per la forzatura, `fino_al` (facoltativo:
dopo quel giorno non vale piu').

**Riservatezza del modo delle corse** (K-011 aggiunta 2, decisioni dell'arbitro):
- la **forzatura** e' un dato comune, sempre visibile; motivo e chi sono personali;
- la **dichiarazione** e' un **dato particolare** (c'e' solo dove l'esclusione e' da confermare, quindi dice il tipo di
  servizio), nella cassaforte; motivo e chi, personali dentro un dato particolare, stanno tra i **legati** (servono tutti e
  due i permessi). Senza i dati particolari la riga del modo non la nomina (JSON `"dichiarazione": "riservato"` su ogni
  corsa) e K-CTR-131 si conta soltanto;
- tutto quello che dipende dalle **scelte per tipo di servizio** (che ci siano, che manchino, cosa dicono) e' un dato
  particolare: senza i dati particolari ogni corsa senza una scelta propria o del suo servizio ha la stessa riga, "modo:
  per un dato particolare" (JSON `"riservato"`), se l'azienda ha almeno una scelta per tipo; con i dati particolari si vede
  tutto, anche "nessuna scelta, decide la legge";
- i **regimi seguiti per scelta** (tachigrafo) di una persona che lavora per un'azienda con una scelta del tachigrafo per
  tipo di servizio (la sua o quella di un suo turno): senza i dati particolari sono riservati tutti (riga del regime, guida
  contata, tipo del riposo, K-CTR-127 e 128 solo contati), con una riga fissa per la persona, sempre: "Regimi seguiti per
  scelta dell'azienda: riservati".

**Chiave della pausa fuori dal posto (rientro)** (decisa dal supervisore il 20/10). Rientro = tra due corse della giornata la
persona (con o senza il mezzo) va in una rimessa (`rimesse.json`) o nel suo luogo di presa servizio, resta li' e torna;
una pausa dove finisce la corsa prima e' "sul posto" e la chiave non la riguarda. La chiave (`rientro` dell'azienda, e
della persona campo per campo) ha il **fermo minimo** in minuti, il **fattore della guida** a vuoto in piu', i **km a
vuoto in piu' al massimo** (facoltativo) e la **forza**. Il rientro e' giustificato se il tempo fermo li' (dall'arrivo
alla partenza) e' almeno il fermo minimo, se e' almeno il fattore per i minuti di guida a vuoto in piu' e, con il
massimo, se i km a vuoto in piu' non lo superano. "In piu'" = i trasferimenti con il mezzo verso il luogo e indietro meno
il trasferimento diretto che servirebbe comunque: zero se la corsa dopo comincia dove finisce quella prima. Altrimenti,
senza distanze, il diretto non si conosce ma non e' mai negativo: giustificato con certezza se basta anche contando
tutti i trasferimenti come in piu'; non giustificato con certezza se manca il fermo minimo; negli altri casi "non
giudicato: servono le distanze" (K-CTR-125), mai "giustificato". Lo stesso per i km (`km_vuoti` delle attivita').

### Come si legge la tabella dei campi

Per ogni file c'e' una tabella: **campo** (con il percorso: `luoghi[].nome` = il nome di ogni luogo
dell'elenco `luoghi`), **obbligatorio** (si'/no), **classe** (comune, personale, particolare) e
**significato**. I campi `schema` e `schema_versione`, presenti in ogni file, non sono ripetuti.

## I file, uno per uno

### pacchetto.json

Il manifesto: chi, quando, da dove vengono i dati, che livello di dati contiene, se ha una cassaforte. Schemi:
[docs/schema/pacchetto.v3.schema.json](schema/pacchetto.v3.schema.json) (versione attuale, K-018: i falsi allarmi),
[docs/schema/pacchetto.v2.schema.json](schema/pacchetto.v2.schema.json) (versione 2, K-010, ancora letta: e' la versione 3 senza
`falsi_allarmi`; per passare alla 3 si cambia solo `"schema_versione"`) e
[docs/schema/pacchetto.v1.schema.json](schema/pacchetto.v1.schema.json) (versione 1, ancora letta: e' la versione 2 senza
`cassaforte` e `porta_a_porta_sensibile`, e il fuso orario senza lunghezza massima; un manifesto 1 vale come "nessuna
cassaforte" e "il porta a porta non e' sensibile"). Per passare alla 2 si scrive `"schema_versione": 2`, si aggiunge
`"cassaforte": false` e si sceglie `porta_a_porta_sensibile`.

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `codice` | si' | comune | Codice del pacchetto (serve a dire da quale pacchetto ne deriva un altro; la cassaforte lo ripete per non finire in un altro pacchetto) |
| `nome` | no | comune | Nome del pacchetto |
| `dati_inventati` | no | comune | `true` = dati inventati (esempi, prove): non usarli come dati reali |
| `aziende[]` | si' | comune | Aziende presenti nel pacchetto (una o piu'): `codice`, `nome` |
| `periodo` | si' | comune | Periodo di validita' dei dati: `dal`, `al` (compresi) |
| `origine.tipo` | si' | comune | `rilevato_a_mano`, `calcolato`, `importato` oppure `scenario` |
| `origine.deriva_da` | no | comune | Codice del pacchetto da cui deriva |
| `origine.nota` | no | comune | Note sull'origine dei dati |
| `livello` | si' | comune | `solo_numeri` (consigliato) oppure `completo` |
| `fuso_orario` | si' | comune | Nome IANA del fuso (es. `Europe/Rome`), al massimo 64 caratteri (versione 2): gli orari sono orari locali di questo fuso |
| `valuta` | si' | comune | Codice ISO 4217 (es. `EUR`): i costi sono in questa valuta |
| `impostazioni.margine_trasbordo_minuti` | si' | comune | Scelta di chi pianifica: la corsa che riceve un trasbordo deve arrivare almeno tanti minuti prima della corsa che consegna |
| `impostazioni.porta_a_porta_sensibile` | si' (v2) | comune | `true` = chi viaggia porta a porta lascia capire un dato sulla salute (es. in Trentino il porta a porta e' di regola un servizio per persone con disabilita'): nome e casa di ogni passeggero porta a porta valgono come dati particolari, come quelli di chi va in un luogo sensibile (vedi "Luoghi sensibili"). `false` = il porta a porta non dice nulla sulla salute (es. trasporto scolastico in campagna) |
| `cassaforte` | si' (v2) | comune | `false` = nessuna cassaforte; altrimenti `id` (codice della cassaforte, 32 cifre esadecimali) e `generazione` (cresce a ogni richiusura). Lo scrive il programma (`kzero chiudi`, `kzero richiudi`): vedi [cassaforte.md](cassaforte.md) |
| `falsi_allarmi[]` | no (v3) | comune | Avvisi sui testi comuni spenti da chi scrive i dati (K-018, sotto): `controllo` (`K-CTR-136`, `K-CTR-137` o `K-CTR-138`), `file`, `campo`, `elemento` (facoltativo), `testo`, `motivo`, `quando` |

**Falsi allarmi** (K-018, versione 3). Gli avvisi sui nomi e sulle parole sulla salute nei testi comuni (docs/controlli.md, "Testi
comuni") possono sbagliare: una via che si chiama come un passeggero, un parcheggio che ha il nome del centro vicino. Chi scrive
i dati spegne l'avviso per quel testo, con il motivo: il controllo, il file, il campo come nella tabella dei campi di
docs/controlli.md (es. `servizi[].nome`, `luoghi[].nomi_alternativi[]`), il codice dell'elemento piu' vicino che ha un codice
(`elemento`, come nel messaggio dell'avviso) e il testo, uguale carattere per carattere. Vale solo per quel controllo, quel campo,
quell'elemento e quel testo: se il testo cambia l'avviso torna, e un falso allarme che non spegne piu' niente ha a sua volta un
avviso. Niente dati personali (come la forzatura del K-011 ma senza "chi"): solo `motivo` (testo comune, che entra anche lui nei
controlli) e `quando`. Sta nel manifesto perche' vale per qualunque file, anche per i testi che non sono di un'azienda (luoghi,
calendari), e perche' lo scrive chi prepara il pacchetto, come le altre impostazioni del manifesto. Il testo spento non e' un
testo libero (ha una sua definizione nello schema): non entra nei controlli, altrimenti si segnalerebbe da solo.

```json
"falsi_allarmi": [
  {
    "controllo": "K-CTR-138",
    "file": "luoghi.json",
    "campo": "luoghi[].nomi_alternativi[]",
    "elemento": "L-SOSTA-VALFONDA",
    "testo": "Valfonda, parcheggio del centro diurno",
    "motivo": "E' il parcheggio dove i pullman aspettano, accanto al centro: nessun passeggero sale o scende qui, quindi non e' un luogo sensibile.",
    "quando": "2026-11-12"
  }
]
```

```json
{
  "schema": "pacchetto",
  "schema_versione": 3,
  "codice": "DEMO-2026",
  "nome": "Pacchetto dimostrativo K-Zero",
  "dati_inventati": true,
  "aziende": [{ "codice": "ALFA", "nome": "Autoservizi Alfa" }, { "codice": "BETA", "nome": "Trasporti Beta" }],
  "periodo": { "dal": "2026-09-01", "al": "2027-08-31" },
  "origine": { "tipo": "scenario", "nota": "Dati inventati per la dimostrazione e le prove." },
  "livello": "completo",
  "fuso_orario": "Europe/Rome",
  "valuta": "EUR",
  "impostazioni": { "margine_trasbordo_minuti": 3, "porta_a_porta_sensibile": true },
  "cassaforte": false
}
```

### aziende.json

L'organizzazione la sceglie ogni azienda: qui stanno i suoi elenchi, con i suoi nomi, e le sue impostazioni.
In un consorzio ogni azienda del manifesto ha la sua riga. Le `impostazioni` del manifesto sono un'altra cosa:
scelte di chi pianifica, valide per tutto il pacchetto. Schemi: versione 3 (K-012: il tipo di servizio `ncc` nel modo delle
corse) [docs/schema/aziende.v3.schema.json](schema/aziende.v3.schema.json), che si legge ancora; versione 4 (K-018: le
parole sulla salute dell'azienda) [docs/schema/aziende.v4.schema.json](schema/aziende.v4.schema.json); versione 2 (K-011, orari
di lavoro), che si legge ancora, [docs/schema/aziende.v2.schema.json](schema/aziende.v2.schema.json); versione 1, che si legge
ancora (senza le categorie di tempo: avviso K-CTR-101), [docs/schema/aziende.v1.schema.json](schema/aziende.v1.schema.json). Per
passare alla 3 o alla 4 si scrive `"schema_versione"`: nient'altro cambia (la 4 aggiunge un campo facoltativo).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `aziende[].azienda` | si' | comune | Codice dell'azienda (dal manifesto); una riga per azienda (errore K-CTR-009 se ripetuta) |
| `aziende[].norme` | no | comune | Norme del paese che valgono per l'azienda (`norme.json`); senza, usi dei mezzi e abilitazioni non si controllano (avviso K-CTR-052) |
| `aziende[].impostazioni.ricovero_vicino_a_casa` | si' | comune | Il mezzo puo' dormire in un ricovero vicino a casa dell'autista: `preferito`, `ammesso`, `mai` (= sempre in rimessa) |
| `aziende[].impostazioni.abbinamento_autista_mezzo` | si' | comune | `nessuno`, `preferito`, `fisso` (ogni autista ha il suo mezzo, il mezzo abituale, e guida solo quello) |
| `aziende[].attivita[]` | no | comune | Tipi di attivita' senza passeggeri dei turni: `codice`, `nome`, `usa_il_mezzo` (true = impegna il mezzo del turno), `tempo` (versione 2, obbligatorio: la categoria di tempo), `nota` |
| `aziende[].impostazioni.soste_non_scritte` | si' (v2) | comune | Versione 2: `categoria` dei minuti non scritti tra due elementi della giornata; con `oltre_minuti` e `categoria_oltre` le soste piu' lunghe hanno un'altra categoria |
| `aziende[].impostazioni.margine_giornate_massime` | no | comune | Versione 2: avviso quando mancano tante giornate al limite delle giornate massime (es. lavoro a chiamata, K-CTR-109) |
| `aziende[].scelte_orari[]` | no | comune | Versione 2: scelte dell'azienda sugli orari (`tipo`, `periodo`, `massimo_minuti`, `minimo_minuti` o `massimo_giorni`, `forza`, `nota`), vedi sopra |
| `aziende[].modo_corse` | no | comune | Versione 2 (K-011 aggiunta 1): OUT o tachigrafo (sopra, "Modo delle corse"): `regime`, `forza`, `tipi_diversi`, `per_tipo_servizio[]` (`tipo_servizio`, `modo`, `forza`, `nota`), `servizi[]` (`servizio`, ...) e `corse[]` (`corsa`, ...) con `modo` (`out`, `tachigrafo`), `forza`, `forzatura` o `dichiarazione` (solo con `out`), `nota`; `nota` |
| `modo_corse.*.forzatura` | no | personale (`motivo`, `chi`), comune (`quando`, `fino_al`) | Chi l'ha scritta, quando e perche'; `fino_al` facoltativo |
| `modo_corse.*.dichiarazione` | no | particolare (K-011 aggiunta 2); `motivo` e `chi` personali dentro un dato particolare (legati: tutti e due i permessi) | Chi l'ha scritta, quando e perche' |
| `aziende[].rientro` | no | comune | Versione 2: chiave del rientro: `fermo_minimo_minuti`, `fattore_guida`, `km_vuoti_massimi` (facoltativo), `forza` |
| `aziende[].speciali_esigenze[]` | no | comune | Versione 2: speciali esigenze dell'azienda per una regola di legge (`regola`, `riferimento`, `nota`), es. per lo straordinario non saltuario della L. 138 |
| `aziende[].tipi_assenza[]` | no | comune | Tipi di assenza delle persone (es. ferie, permesso, formazione): `codice`, `nome`, `nota`. Mai la causa |
| `aziende[].tipi_fermo[]` | no | comune | Tipi di fermo dei mezzi (es. revisione, manutenzione): `codice`, `nome`, `nota` |
| `aziende[].competenze[]` | no | comune | Competenze che un turno o un servizio puo' richiedere (es. conosce la linea B610, formato per la pedana): `codice`, `nome`, `nota` |
| `aziende[].parole_sulla_salute[]` | no (v4) | comune | K-018: parole ed espressioni in piu' di quelle delle norme del paese che per l'azienda lasciano capire un dato sulla salute (es. il nome di un centro, una sigla usata in azienda): per l'avviso K-CTR-137 sui testi dell'azienda e su quelli che non sono di un'azienda (docs/controlli.md, "Testi comuni") |
| `aziende[].nota` | no | comune | Note |

Le impostazioni sono solo dati: il costo delle scelte lo mostrera' il calcolo. Nel codice non c'e' nessun tipo di
attivita', assenza, fermo o competenza: sono nomi nei dati. Nel demo (K-011) le categorie dubbie sono scelte per
prudenza: `ATTESA` (attesa con il mezzo) e `SOSTA` (sosta al capolinea di Beta) sono disponibilita' e non interruzione,
perche' il conducente resta con il mezzo e non e' libero da ogni servizio (per il 561 la disponibilita' non e' una
interruzione, art. 4 d; per la L. 138 e' lavoro effettivo, art. 6; per il 234 non e' orario di lavoro, art. 3); `RICARICA`
e `RISERVA` disponibilita'; `A-PIEDI` altre mansioni (spostarsi per il servizio e' lavoro). I codici delle competenze si confrontano come testi: in un
consorzio le aziende che si scambiano persone usano gli stessi codici per le stesse competenze.

### norme.json

Le norme di un paese che servono a controllare chi guida cosa e quale mezzo fa quale servizio e, dalla versione 2
(K-011), le leggi e i contratti collettivi sugli orari di lavoro. **Nessuna patente, nessun uso e nessun limite di
orario e' scritto nel programma**: tutto e' un dato con la fonte, e cio' che non e' certo si segna `da_confermare`.
Schemi: versione 4 (K-018) [docs/schema/norme.v4.schema.json](schema/norme.v4.schema.json): le parole sulla salute del paese
(`parole_sulla_salute`); versione 3 (K-012), che si legge ancora (senza l'elenco: nessun avviso sulle parole sulla salute da
queste norme), [docs/schema/norme.v3.schema.json](schema/norme.v3.schema.json): il tipo di servizio `ncc` e i
servizi occasionali di un regime tipo per tipo; versione 2, che si legge ancora (un solo `da_confermare` per tutti i
servizi occasionali: vale per ogni tipo dell'elenco, e `altro` resta `altro`),
[docs/schema/norme.v2.schema.json](schema/norme.v2.schema.json); versione 1, che si legge ancora (nessun regime: per i
dipendenti l'avviso K-CTR-107), [docs/schema/norme.v1.schema.json](schema/norme.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `norme[].codice` | si' | comune | Codice delle norme (es. `N-IT`) |
| `norme[].nome` | si' | comune | Nome |
| `norme[].paese` | si' | comune | Paese, codice ISO 3166-1 di due lettere |
| `norme[].abilitazioni[]` | no | comune | Elenco delle abilitazioni del paese: `codice`, `nome`, `fonte`, `nota` (in Italia patenti B, D1, D, DE, CQC persone, KB, carta del conducente; in Germania anche il "P-Schein") |
| `norme[].usi[]` | no | comune | Elenco degli usi dei mezzi come li scrive la carta di circolazione: `codice`, `nome`, `fonte`, `nota` (in Italia servizio di linea, noleggio con conducente, uso proprio) |
| `norme[].requisiti_guida[]` | no | comune | Requisiti per guidare: `categorie` (M1, M2, M3), `posti_da`, `posti_fino_a`, `rimorchio`, `usi` (facoltativi) e le `alternative`; `fonte`, `da_confermare`, `nota` |
| `requisiti_guida[].alternative[]` | si' | comune | Modi di soddisfare il requisito (ne basta uno): `abilitazioni[]` da avere tutte insieme; facoltativi `posti_fino_a` e `lunghezza_fino_a_m` (es. la patente D1 fino a 16 posti e 8 m) |
| `norme[].usi_per_servizio[]` | no | comune | Usi dei mezzi ammessi per tipo di servizio: `tipi_servizio`, `usi` (ne basta uno), `fonte`, `da_confermare`, `nota` |
| `norme[].regimi[]` | no | comune | Versione 2: leggi e contratti collettivi sugli orari (sopra, "Orari di lavoro"): `codice`, `nome`, `tipo` (`legge`, `contratto_collettivo`), `fonte`, `valido_dal`, `valido_al`, `da_confermare`, `nota`, `si_applica`, `esclusioni`, `servizi_occasionali`, `estensione`, `esclude`, `casi_non_trattati`, `regole` |
| `regimi[].si_applica` | no | comune | `ruoli`, `rapporti`, `contratti_collettivi`, `tipi_servizio` (ognuno con i valori `certi` e quelli `da_confermare`), `mezzi` (`persone_trasportabili_oltre`), `percorso` (`fino_a_km`, `oltre_km`), `come_regime` (le condizioni sulla corsa di un altro regime) con `estensione_come_regime` (`vale` o `da_confermare`, di base); senza una condizione vale per tutti |
| `regimi[].servizi_occasionali` | no | comune | Tipi di servizio che sono servizi occasionali per le regole che valgono solo per loro (es. l'interruzione di 15 + 15 minuti del Reg. 561/2006, art. 7). Versione 3 (K-012): `tipi_servizio` con i `certi` e quelli `da_confermare`, come in `si_applica`, e la `fonte`; un servizio occasionale da confermare da' avvisi con il motivo, mai un errore. Versione 2: `tipi_servizio` (elenco) e un solo `da_confermare` per tutti |
| `regimi[].esclusioni[]` | no | comune | Corse escluse: `tipi_servizio` (ogni tipo della corsa deve essere escluso), `insieme_a` (la voce vale solo se la corsa ha anche uno di questi tipi), `percorso_fino_a_km`, `da_confermare`, `fonte`, `nota` |
| `regimi[].guida_delle_corse_escluse` | no | comune | Versione 2 (K-011 aggiunta 1): come contano per le regole di guida del regime i minuti di guida delle corse escluse: `conta_come` (lettura certa) e `prudente` (`guida`, `altre_mansioni`), `fonte`, `nota` |
| `regimi[].estensione` | no | comune | `ambito` `settimana` e le `famiglie` di regole (`guida`, `riposi`, `lavoro`, `notturno`, `giornate`) per cui una corsa vale tutta la settimana |
| `regimi[].esclude[]` | no | comune | Altri regimi esclusi (o lasciati da confermare) per alcune famiglie nei giorni o nelle settimane in cui questo vale con certezza: `regime`, `famiglie`, `effetto` (`esclude`, `da_confermare`) |
| `regimi[].regole[]` | si' | comune | Regole: `codice` (unico nelle norme), `tipo`, `famiglia`, `fonte`, `periodo`, `misura`, i campi del tipo, `prudente`, `da_confermare`, `nota` |
| `norme[].cambi_ora` | no | comune | Versione 2: `fonte` e `date` (`data`, `da`, `a`) dei cambi dell'ora del paese |
| `norme[].parole_sulla_salute[]` | no (v4) | comune | K-018: parole ed espressioni che nel paese lasciano capire un dato sulla salute (sotto), per gli avvisi K-CTR-137 e K-CTR-138 |
| `norme[].nota` | no | comune | Note |

Come si usano:
- **requisiti di guida**: per il mezzo di un turno valgono tutti i requisiti che gli corrispondono (categoria, posti
  per i passeggeri, rimorchio e, se il requisito ha degli usi, l'uso della carta di circolazione o un uso autorizzato
  del mezzo). Per ognuno l'autista deve avere tutte le abilitazioni di almeno un'alternativa i cui limiti valgono per
  quel mezzo (un mezzo senza la lunghezza non rispetta un limite di lunghezza: prudenza). Valgono le norme
  dell'azienda del turno;
- **usi per tipo di servizio**: per una corsa vale prima il regolamento del contratto del servizio (parametro
  `usi_dei_mezzi`), poi le norme dell'azienda del contratto. Senza regola, nessun uso e' richiesto. Il mezzo va bene
  se il suo `uso` e' tra quelli ammessi, oppure se ha un'autorizzazione a un uso ammesso valida quel giorno (la
  distrazione, vedi `mezzi.json`).

**Parole sulla salute** (K-018, versione 4). Le parole e le espressioni che, scritte in un testo comune (un nome, un codice, una
nota), lasciano capire un dato sulla salute: si confrontano per parole intere, senza maiuscole ne' accenti (una voce di due parole,
es. "centro diurno", le cerca una dopo l'altra). Sono un dato, come le regole: il programma non ne conosce nessuna. Valgono per i
testi delle aziende che seguono queste norme e per i testi che non sono di un'azienda; un'azienda ne aggiunge altre in
`aziende.json` (versione 4). I testi delle norme e dei regolamenti non si controllano: sono regole pubbliche con la fonte
(marcatura `x-kzero-regole-pubbliche` dei loro schemi). Nel demo (N-IT): disabili, disabile, handicap, centro diurno, terapia,
riabilitazione, GSH, individualizzato.

Nel demo le norme italiane (`N-IT`): patenti e usi dal Codice della strada (artt. 82 e 116), la CQC dal D.Lgs.
286/2005; il certificato KB per le autovetture a noleggio e la patente DE sono segnati "da confermare". Dalla versione 2
anche i regimi sugli orari R-L138 (L. 138/1958), R-561 (Reg. CE 561/2006), R-234 (D.Lgs. 234/2007), R-CHIAMATA (D.Lgs.
81/2015 art. 13) e il contratto collettivo inventato ESEMPIO-01, con le fonti e le letture da confermare (REGOLE.md,
"Orari di lavoro").

### luoghi.json

Tutti i luoghi: `luoghi` (pubblici) e `luoghi_privati` (case di persone, dati personali). Schema:
[docs/schema/luoghi.v2.schema.json](schema/luoghi.v2.schema.json); la versione 1
([docs/schema/luoghi.v1.schema.json](schema/luoghi.v1.schema.json), senza `sensibile`) si legge ancora: per passare alla
2 si scrive `"schema_versione": 2` e si segnano i luoghi sensibili.

Tipi dei luoghi pubblici: `fermata` (fermata pubblica), `scuola`, `centro` (diurno, terapie, socio-educativo...),
`rimessa`, `ricovero_esterno`, `punto_sosta` (dove il mezzo aspetta), `punto_trasbordo` (punto di trasbordo
autorizzato). Tipi dei luoghi privati: `porta_a_porta` (la casa del passeggero, dove sale e scende) e
`ricovero_esterno` (es. il garage presso la casa di un autista).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `luoghi[].codice` | si' | comune | Codice del luogo |
| `luoghi[].tipo` | si' | comune | Tipo di luogo pubblico (elenco sopra) |
| `luoghi[].nome` | si' | comune | Nome ufficiale |
| `luoghi[].nomi_alternativi[]` | no | comune | Altri nomi con cui il luogo compare (schede dei committenti, orari...) |
| `luoghi[].comune` | si' | comune | Comune: servira' a dividere il lavoro per zone |
| `luoghi[].coordinate` | no | comune | Posizione (senza coordinate: avviso K-CTR-035) |
| `luoghi[].codici_esterni[]` | no | comune | Codici del luogo in altri sistemi |
| `luoghi[].limiti_accesso` | no | comune | Misure e massa massime (`lunghezza_massima_m`, `larghezza_massima_m`, `altezza_massima_m`, `massa_massima_t`) e `divieti[]` |
| `luoghi[].limiti_accesso.divieti[]` | no | comune | Divieto per un `mezzo` (codice) oppure per una `categoria` (M1, M2, M3), con il `motivo` (`cartello`, `esperienza` dell'autista, `altro`) e una `nota` |
| `luoghi[].trasbordo` | no | comune | Presente = punto di trasbordo autorizzato: `autorizzato_da` (es. "il consorzio, sentita la Provincia"), `data`, `documento` oppure `a_voce: true` (avviso K-CTR-043), `nota` |
| `luoghi[].sensibile` | no | comune | Versione 2: `true` = luogo sensibile, cioe' dire che una persona ci va lascia capire un dato sulla salute (es. un centro diurno, un centro di terapia). Lo sceglie chi scrive i dati: un doposcuola, anch'esso di tipo `centro`, di solito non lo e' (sotto) |
| `luoghi[].nota` | no | comune | Note |
| `luoghi_privati[].codice` | si' | comune | Codice del luogo (resta visibile: e' quello che compare al posto dei dati personali) |
| `luoghi_privati[].tipo` | si' | comune | `porta_a_porta` oppure `ricovero_esterno` |
| `luoghi_privati[].nome` | no | personale | Nome (es. "Casa Bianchi") |
| `luoghi_privati[].indirizzo` | no | personale | Indirizzo |
| `luoghi_privati[].comune` | no | personale | Comune |
| `luoghi_privati[].coordinate` | no | personale | Posizione |
| `luoghi_privati[].codici_esterni[]` | no | personale | Codici in altri sistemi |
| `luoghi_privati[].nota` | no | personale | Note |

Un punto di trasbordo e' un luogo di tipo `punto_trasbordo`, oppure un altro luogo pubblico (es. un punto di
sosta) con il blocco `trasbordo`. Una casa non e' mai un punto di trasbordo.

**Luoghi sensibili** (K-009; regola nuova dal K-010). Il nome e la posizione di un centro diurno sono dati comuni (e' un
luogo pubblico), ma "Bianchi Luca scende al centro diurno", o "la corsa passa da Casa Bianchi e arriva al centro diurno",
e' un dato sulla salute di chi ci va. Dal K-010 la regola vale **per la persona, non per il luogo**: il nome, la casa e
gli altri dati personali di un passeggero **legato** a un luogo sensibile, e i dati della sua casa, valgono come **dati
particolari**, in **tutte le uscite** (non solo dove compare il centro); il centro si vede sempre. Si vedono solo
insieme ai dati particolari (`--includi-dati-particolari`, e con la cassaforte i permessi dei dati personali e
particolari); altrimenti al loro posto c'e' il codice, che nessuna uscita lega mai al nome. Cosi' la regola regge tra
uscite diverse: chi ha i soli dati personali non puo' leggere il nome in un'uscita e il centro accanto al codice della
casa in un'altra. Nella cassaforte quei dati stanno in una sezione a parte, quella dei legati, che si decifra solo con
tutti e due i permessi: chi ne ha uno solo (i personali o i particolari) non li puo' decifrare.

Chi e' legato (calcolato sul pacchetto, dai dati comuni; una regola sola nel codice, `LegameSensibile`):
- i passeggeri di un gruppo che va (o parte) da una scuola o un centro in un luogo sensibile, o che ha una fermata in un
  luogo sensibile; i passeggeri che salgono o scendono in un luogo sensibile;
- le case (punti porta a porta) dei passeggeri legati e quelle tra le fermate di un gruppo legato; le case toccate da
  una corsa che, da sola o con i suoi trasbordi, passa da un luogo sensibile; e i passeggeri che salgono o scendono in
  una casa legata;
- con `porta_a_porta_sensibile: true` nel manifesto (versione 2): tutte le case porta a porta e i loro passeggeri;
- (K-012) i committenti privati con un servizio legato: gruppi, fermate o passeggeri in un luogo sensibile, una corsa che
  passa da un luogo sensibile (da sola o con i trasbordi) o, con `porta_a_porta_sensibile`, case porta a porta (vedi
  committenti.json).

Non riguarda il personale (lavorare in un centro, o il garage presso la casa di un autista, non e' un dato sulla salute
di nessuno) ne' i conteggi. Nel demo il centro diurno `L-CENTRO` e' sensibile e il porta a porta e' sensibile. I nomi e
i codici liberi (servizi, gruppi, corse, turni, calendari, note) restano responsabilita' di chi scrive i dati: un nome
come "Trasporto al centro diurno" accanto all'elenco dei passeggeri dice lo stesso (docs/sicurezza.md).

```json
{
  "codice": "F-PRADELLO",
  "tipo": "fermata",
  "nome": "Pradello, Piazza",
  "nomi_alternativi": ["Pradello P.zza", "Pradello Chiesa"],
  "comune": "Pradello",
  "coordinate": { "lat": 46.4421, "lon": 11.0712 },
  "codici_esterni": [{ "sistema": "Trasporti Provinciali Valfonda", "tipo": "fermata", "codice": "5101" }]
}
```

### tratti.json

Tratti di strada tra due luoghi con i loro limiti (es. un sottopasso, una strada stretta). Schema:
[docs/schema/tratti.v1.schema.json](schema/tratti.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `tratti[].codice` | si' | comune | Codice del tratto |
| `tratti[].nome` | no | comune | Nome (es. "Sottopasso ferroviario di Montebrino") |
| `tratti[].da`, `tratti[].a` | si' | comune | Luoghi alle due estremita' |
| `tratti[].entrambi_i_sensi` | si' | comune | `true` = i limiti valgono nei due sensi |
| `tratti[].limiti` | si' | comune | Come i limiti di accesso dei luoghi: misure, massa, divieti |
| `tratti[].nota` | no | comune | Note |

```json
{
  "codice": "T-SOTTOPASSO-MONTEBRINO",
  "nome": "Sottopasso ferroviario di Montebrino",
  "da": "F-CASTELROVO",
  "a": "F-MONTEBRINO",
  "entrambi_i_sensi": true,
  "limiti": { "altezza_massima_m": 3.2, "divieti": [{ "categoria": "M3", "motivo": "cartello" }] }
}
```

### regolamenti.json

Un regolamento e' l'insieme dei parametri di un territorio o di un committente (es. il testo unico di una
Provincia). **Il programma non conosce nessun valore: li legge da qui.** Ogni contratto indica il
regolamento che vale per i suoi servizi. Schemi: [docs/schema/regolamenti.v3.schema.json](schema/regolamenti.v3.schema.json)
(versione attuale, K-012: il tipo di servizio `ncc` negli elenchi dei tipi; per passare alla 3 si cambia solo
`"schema_versione"`), [docs/schema/regolamenti.v2.schema.json](schema/regolamenti.v2.schema.json) (versione 2, ancora
letta) e [docs/schema/regolamenti.v1.schema.json](schema/regolamenti.v1.schema.json) (versione 1, ancora
letta: e' la versione 2 senza `usi_dei_mezzi` e `autorizzazione_del_committente`).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `regolamenti[].codice` | si' | comune | Codice del regolamento |
| `regolamenti[].ente` | si' | comune | Ente che lo ha scritto |
| `regolamenti[].titolo` | si' | comune | Titolo |
| `regolamenti[].documento` | si' | comune | Documento ufficiale (es. "Delibera della Giunta provinciale n. 2 del 10/01/2025") |
| `regolamenti[].valido_dal`, `valido_al` | no | comune | Date di validita' (un contratto scoperto per una parte del periodo: avviso K-CTR-046) |
| `regolamenti[].parametri` | si' | comune | I parametri, tutti facoltativi (tabella sotto) |
| `regolamenti[].nota` | no | comune | Note |

Ogni parametro ha la sua **fonte**: `fonte.punto` (obbligatorio, es. "1.B.1"), `fonte.documento` (se diverso
da quello del regolamento) e `fonte.riferimenti` (altre norme citate), piu' una `nota` facoltativa.

| Parametro | Forma | Significato |
|---|---|---|
| `attesa_massima_prima_minuti` | `valore` | Attesa massima a scuola prima dell'inizio delle lezioni |
| `attesa_massima_dopo_minuti` | `valore` | Attesa massima dopo la fine delle lezioni |
| `distanza_massima_casa_fermata_m` | `valore` | Distanza massima casa-fermata (metri) |
| `distanza_massima_fermata_scuola_m` | `valore` | Distanza massima fermata-scuola (metri) |
| `distanza_minima_tra_fermate_m` | `valore` | Distanza minima tra due fermate (metri) |
| `distanza_minima_casa_scuola_m` | `valori[]` per grado | Distanza minima casa-scuola per avere il trasporto |
| `alunni_minimi_per_servizio` | `valori[]` per grado | Alunni minimi per attivare un servizio |
| `durata_massima_a_bordo_minuti` | `valori[]` per grado | Durata massima a bordo |
| `assistente_a_bordo` | `condizioni[]` | Quando serve l'assistente a bordo |
| `adulto_alla_discesa` | `condizioni[]` | Quando serve un adulto alla fermata di discesa |
| `discesa_senza_adulto_con_autorizzazione` | `gradi[]` | Gradi per cui la discesa senza adulto e' ammessa con l'autorizzazione dei genitori |
| `passeggeri_in_piedi` | `non_ammessi_per_gradi[]`, `durata_massima_minuti`, `km_massimi` | Passeggeri in piedi |
| `combinazioni_vietate` | `combinazioni[]` di `primo` e `secondo` | Passeggeri che non possono viaggiare insieme (per grado o tipo di gruppo) |
| `usi_dei_mezzi` | `valori[]` di `tipi_servizio` e `usi` | Versione 2. Usi dei mezzi ammessi per tipo di servizio, se il territorio decide diversamente dalle norme del paese (codici degli usi di `norme.json`) |
| `autorizzazione_del_committente` | `tipi_servizio[]` | Versione 2. Per questi tipi di servizio l'autista deve avere un'autorizzazione del committente del contratto (es. un cartellino di riconoscimento) |

`valori[]` per grado: elenco di `{ "gradi": [...], "valore": n }`. Gradi: `infanzia`, `primaria`,
`secondaria_primo_grado`, `secondaria_secondo_grado`.
`condizioni[]`: basta che ne valga una; una condizione vale se a bordo c'e' almeno uno dei `gradi`, dei
`tipi_gruppo`, delle `esigenze`, dei `tipi_servizio` e dei `tipi_centro` elencati (un elenco che manca non
pone condizioni) e, se c'e' `posti_passeggeri_oltre`, il mezzo ha piu' posti per i passeggeri di quel numero
(conducente escluso).

```json
"assistente_a_bordo": {
  "condizioni": [{ "gradi": ["infanzia"], "posti_passeggeri_oltre": 8 }],
  "fonte": { "punto": "1.C1.9", "riferimenti": "DM 31/01/1997" },
  "nota": "Il testo dice 'mezzi con piu' di 9 posti': letto come 9 posti compreso il conducente. Da confermare."
}
```

Il pacchetto demo ha due regolamenti: `R-TN` con i valori pubblici del testo unico della Provincia di Trento
(vedi REGOLE.md, parte 2) e `R-INV` con valori inventati e diversi, usato da un altro contratto: lo stesso
controllo con i due regolamenti da' esiti diversi (prova `due_regolamenti_*`).

### festivita.json

Le festivita' di un paese o di un territorio (le feste nazionali, il santo patrono di un comune), con la fonte.
**Nessuna festivita' e' scritta nel programma, nemmeno quelle mobili come Pasqua**: le date sono dati, e un
calendario le aggiunge o le toglie (sotto). Cosi' le feste si scrivono una volta sola e valgono per tutti i
calendari che le usano. Schema: [docs/schema/festivita.v1.schema.json](schema/festivita.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `festivita[].codice` | si' | comune | Codice del calendario delle festivita' |
| `festivita[].nome` | si' | comune | Nome (es. "Festivita' nazionali in Italia, 2026/27") |
| `festivita[].paese` | si' | comune | Paese, codice ISO 3166-1 di due lettere (`IT`, `DE`, `CH`...) |
| `festivita[].territorio` | no | comune | Territorio a cui vale (es. un comune); assente = tutto il paese |
| `festivita[].fonte` | si' | comune | `documento` (obbligatorio), `punto`, `riferimenti`: da dove vengono le date |
| `festivita[].date[]` | si' | comune | Le feste: `data`, `nome`, `da_confermare` (facoltativo), `nota` |
| `festivita[].nota` | no | comune | Note |

```json
{
  "codice": "FEST-VALFONDA",
  "nome": "Santo patrono di Valfonda (San Martino)",
  "paese": "IT",
  "territorio": "Comune di Valfonda (inventato)",
  "fonte": { "documento": "Statuto del Comune di Valfonda (inventato)", "punto": "art. 3" },
  "date": [{ "data": "2026-11-11", "nome": "San Martino, patrono di Valfonda" }]
}
```

### calendari.json

Quando vale un servizio, una corsa o un turno. Schemi: [docs/schema/calendari.v2.schema.json](schema/calendari.v2.schema.json)
(versione attuale) e [docs/schema/calendari.v1.schema.json](schema/calendari.v1.schema.json) (versione 1, ancora letta:
e' la versione 2 senza `aggiunge` e `toglie`).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `calendari[].codice` | si' | comune | Codice del calendario |
| `calendari[].nome` | no | comune | Nome |
| `calendari[].codici_esterni[]` | no | comune | Codici in altri sistemi (es. i periodi "o", "f1"... di un committente) |
| `calendari[].base` | no | comune | Calendario di base: questo calendario vale solo nei giorni in cui vale anche la base |
| `calendari[].dal`, `calendari[].al` | si' | comune | Primo e ultimo giorno |
| `calendari[].giorni[]` | si' | comune | Giorni della settimana: `lunedi` ... `domenica` (anche nessuno, se si usano solo date aggiunte) |
| `calendari[].date_aggiunte[]` | no | comune | Date in cui vale anche se non e' uno dei giorni indicati |
| `calendari[].date_tolte[]` | no | comune | Date in cui non vale |
| `calendari[].sospensioni[]` | no | comune | Periodi in cui non vale: `dal`, `al`, `motivo` (es. "chiusura del centro") |
| `calendari[].aggiunge` | no | comune | Versione 2. Giorni di altri calendari (`calendari[]`) o di calendari delle festivita' (`festivita[]`) in cui questo calendario vale, dentro il suo periodo |
| `calendari[].toglie` | no | comune | Versione 2. Giorni di altri calendari (`calendari[]`) o di calendari delle festivita' (`festivita[]`) in cui questo calendario non vale |
| `calendari[].nota` | no | comune | Note |

Come si decide se un calendario vale in un giorno (il primo punto che decide vince):
1. le date tolte e le sospensioni prevalgono su tutto: il calendario non vale;
2. le date aggiunte: vale (sono una scelta precisa dell'utente, quindi valgono anche contro un calendario tolto);
3. fuori dal periodo (`dal`, `al`) non vale;
4. un giorno di un calendario tolto (`toglie`): non vale;
5. un giorno di un calendario aggiunto (`aggiunge`): vale, anche se non e' uno dei giorni della settimana;
6. altrimenti il giorno deve essere uno dei giorni della settimana e, se c'e' una base, valere anche per la base.

Con questi elementi si scrivono una volta sola: i periodi di un committente; "feriali" (da lunedi' a sabato,
`toglie` le festivita'); "festivi" (`giorni` = domenica, `aggiunge` le festivita'); "scolastici" (base = giorni di
scuola); "estivo" e "invernale" (periodi); "solo nei giorni scolastici escluso il sabato" (base = giorni di scuola,
giorni = da lunedi' a venerdi'); l'attivazione o la disattivazione di un servizio (periodo o sospensione); i giorni
sparsi (lunedi', mercoledi', giovedi'). Un calendario che si basa su se stesso (K-CTR-016) o che si combina con se
stesso (K-CTR-049), direttamente o attraverso altri, e' un errore.

```json
{
  "codice": "CAL-FERIALI",
  "nome": "Giorni feriali (da lunedi' a sabato, festivi esclusi)",
  "dal": "2026-09-01",
  "al": "2027-08-31",
  "giorni": ["lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato"],
  "toglie": { "festivita": ["FEST-IT"] }
}
```

```json
{
  "codice": "CAL-CENTRO",
  "nome": "Centro diurno: lunedi', mercoledi' e giovedi'",
  "dal": "2026-09-01",
  "al": "2027-07-30",
  "giorni": ["lunedi", "mercoledi", "giovedi"],
  "date_aggiunte": ["2026-12-22"],
  "date_tolte": ["2027-06-02"],
  "sospensioni": [{ "dal": "2026-12-24", "al": "2026-12-31", "motivo": "Chiusura del centro" }]
}
```

### scuole.json

Scuole e centri (prima "destinazioni"). Schema: [docs/schema/scuole.v1.schema.json](schema/scuole.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `scuole[].codice` | si' | comune | Codice della scuola o del centro |
| `scuole[].nome` | si' | comune | Nome |
| `scuole[].luogo` | si' | comune | Luogo (di tipo `scuola` o `centro`) |
| `scuole[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `scuole[].tipo` | si' | comune | `scuola` oppure `centro` |
| `scuole[].grado` | per le scuole | comune | `infanzia`, `primaria`, `secondaria_primo_grado`, `secondaria_secondo_grado` |
| `scuole[].tipo_centro` | per i centri | comune | `valore`: `diurno`, `terapie`, `socio_educativo` oppure `altro` con `descrizione` |
| `scuole[].orari[]` | si' | comune | Orari per periodo e giorni della settimana (sotto) |
| `scuole[].durata_massima_a_bordo_minuti` | no | comune | Durata massima a bordo per chi va in questa scuola |
| `scuole[].salita_non_prima_di` | no | comune | Ora piu' presto di salita |
| `scuole[].nota` | no | comune | Note |
| `orari[].dal`, `orari[].al`, `orari[].giorni[]` | si' | comune | Periodo e giorni a cui vale l'orario |
| `orari[].inizio`, `orari[].fine` | si' | comune | Inizio e fine delle lezioni, o apertura e chiusura |
| `orari[].arrivo_entro` | no | comune | Ora di arrivo richiesta (es. centro: arrivo entro le 09:00) |
| `orari[].attesa_concordata` | no | comune | `prima_minuti`, `dopo_minuti` accettati dalla scuola, con il riferimento dell'`accordo`; devono stare dentro i limiti del regolamento del contratto (errore K-CTR-021) |
| `orari[].orari_autorizzati[]` | no | comune | Orari di `arrivo` o di `ripresa` autorizzati, con il riferimento dell'`accordo` |

### committenti.json

Chi affida i servizi: un **ente** (un consorzio, una Provincia, un Comune, una scuola, un'associazione, un'agenzia) oppure,
dalla versione 2 (K-012), un **privato** (una persona, es. chi prenota un noleggio per un matrimonio). Schemi:
[docs/schema/committenti.v2.schema.json](schema/committenti.v2.schema.json) (versione attuale) e
[docs/schema/committenti.v1.schema.json](schema/committenti.v1.schema.json) (versione 1, ancora letta: e' la versione 2 senza
`committenti_privati`; per passare alla 2 si cambia solo `"schema_versione"`).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `committenti[].codice` | si' | comune | Codice del committente (un ente) |
| `committenti[].nome` | si' | comune | Nome (es. un consorzio, una Provincia, un Comune, un'associazione) |
| `committenti[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `committenti[].nota` | no | comune | Note |
| `committenti_privati[].codice` | si' | comune | Versione 2. Codice del committente privato: resta visibile al posto del nome (unico con i codici degli enti) |
| `committenti_privati[].nome` | no | personale | Nome e cognome della persona |
| `committenti_privati[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `committenti_privati[].nota` | no | personale | Note |

**Committenti privati** (K-012). Per un privato il nome e la nota sono **dati personali**: nella cassaforte, di base sostituiti
dal codice nelle uscite (`kzero giornata` e `kzero dimostrazione` mostrano il nome del committente dal K-012), mai nel pacchetto
"solo numeri" (lo dice K-CTR-019: il committente resta, con il solo codice). Per un ente restano comuni, come prima. Il formato
non ha campi per telefono, mail o codice fiscale e non li avra'. Il nome **non si perde mai**: chi ha il permesso dei dati
personali lo vede sempre (se il privato e' legato a un luogo sensibile, chi ha i permessi dei dati personali e particolari);
gli altri vedono il codice. Le classi restano campo per campo, ricavate dagli schemi: i privati stanno in un **elenco a parte**,
come i luoghi privati, invece di un campo che cambia classe secondo un altro valore (DECISIONI.md, K-012).

**Il legame con la salute**: il nome di un privato segue la stessa regola dei passeggeri (sotto, "Luoghi sensibili" in
luoghi.json): se un servizio di un suo contratto ha gruppi, fermate o passeggeri in un luogo sensibile, ha una corsa che passa
da un luogo sensibile (anche con i trasbordi) o, con `porta_a_porta_sensibile`, ha case porta a porta, i suoi dati personali
sono **legati**: si vedono solo con i dati personali e particolari insieme (nella cassaforte, la sezione dei legati).
Altrimenti sono solo personali, come il nome di un passeggero di un servizio per disabili diretto a una scuola (il tipo del
servizio resta un dato particolare).

**Restano dati comuni, scelti dall'utente**: il codice del committente e i suoi codici esterni, il codice e il nome dei suoi
servizi e dei suoi contratti. **Non devono contenere il nome del privato** (es. non "Matrimonio Rossi", ma "Noleggio per un
matrimonio"; non il codice "ROSSI"): sono dati comuni e si vedono in tutte le uscite. L'avviso automatico sui nomi liberi
arrivera' con il suo mandato.

### contratti.json

Il contratto lega un committente e un'azienda e dice le regole che valgono per i suoi servizi.
Senza importi: i soldi saranno un mandato a parte. Schema: [docs/schema/contratti.v1.schema.json](schema/contratti.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `contratti[].codice` | si' | comune | Codice del contratto |
| `contratti[].nome` | no | comune | Nome |
| `contratti[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `contratti[].committente` | si' | comune | Committente |
| `contratti[].azienda` | si' | comune | Azienda che svolge i servizi |
| `contratti[].regolamento` | si' | comune | Regolamento che vale per i servizi del contratto |
| `contratti[].valido_dal`, `valido_al` | no | comune | Date di validita' |
| `contratti[].assistente_fornito_da` | si' | comune | `committente` (per l'azienda l'assistente occupa solo un posto) oppure `azienda` (va pianificato e pagato come l'autista) |
| `contratti[].regole.posti_minimi` | no | comune | Mezzo minimo: posti per i passeggeri |
| `contratti[].regole.posti_massimi` | no | comune | Mezzo massimo ammesso (piu' piccolo del minimo: errore K-CTR-022) |
| `contratti[].regole.pedana_obbligatoria` | no | comune | Pedana obbligatoria su tutti i servizi del contratto |
| `contratti[].regole.durata_massima_a_bordo_minuti` | no | comune | Durata massima a bordo |
| `contratti[].regole.categoria_riconosciuta` | no | comune | Categoria richiesta dal committente: un mezzo piu' grande non da' diritto a una categoria piu' alta |
| `contratti[].nota` | no | comune | Note |

### gruppi_servizi.json

Servizi affidati insieme con lo stesso contratto (es. un "lotto"): di solito un servizio si perde o si
scambia una corsa alla volta, a volte un intero lotto. Schema:
[docs/schema/gruppi_servizi.v1.schema.json](schema/gruppi_servizi.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `gruppi_servizi[].codice` | si' | comune | Codice del gruppo |
| `gruppi_servizi[].nome` | no | comune | Nome (es. "Lotto 2 Valle Alta") |
| `gruppi_servizi[].contratto` | si' | comune | Contratto: tutti i suoi servizi devono avere questo contratto (errore K-CTR-032) |
| `gruppi_servizi[].servizi[]` | si' | comune | Servizi del gruppo (un servizio sta in un solo gruppo: errore K-CTR-033) |
| `gruppi_servizi[].nota` | no | comune | Note |

### servizi.json

Il **servizio** e' il lavoro che un committente affida con un contratto: si paga, si perde, si rinuncia,
si scambia. La sua **domanda** dice chi deve andare da dove a dove ed entro quando, indipendente dalle corse
di oggi: cosi' un calcolo futuro potra' rifare i giri con mezzi diversi. Schema:
[docs/schema/servizi.v3.schema.json](schema/servizi.v3.schema.json) (versione attuale, K-012: il tipo `ncc`; per passare alla
3 si cambia solo `"schema_versione"`), [docs/schema/servizi.v2.schema.json](schema/servizi.v2.schema.json) (versione 2,
ancora letta: un servizio `altro` resta `altro`, mai `ncc` da solo) e
[docs/schema/servizi.v1.schema.json](schema/servizi.v1.schema.json) (versione 1, ancora letta: e' la versione 2 senza
`competenze_richieste`).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `servizi[].codice` | si' | comune | Codice del servizio |
| `servizi[].nome` | no | comune | Nome |
| `servizi[].codici_esterni[]` | no | comune | Es. sistema "Consorzio", tipo "servizio", codice "1234567/1"; scheda handicap "12345" |
| `servizi[].tipo` | si' | particolare | Tipo dichiarato dal committente: `linea`, `scolastico`, `disabili`, `ncc` (versione 3, sotto), `altro` (con `tipo_descrizione`). "Promiscuo" non si dichiara: lo ricava il programma sulla corsa |
| `servizi[].tipo_descrizione` | con `altro` | particolare | Descrizione del tipo |
| `servizi[].contratto` | si' | comune | Contratto |
| `servizi[].calendario` | si' | comune | Calendario |
| `servizi[].liberta_oraria.tipo` | si' | comune | `fissa` (orari del committente, es. l'orario pubblico di una linea) oppure `libera` (liberi dentro il regolamento) |
| `servizi[].liberta_oraria.soglia_da_concordare_minuti` | con `libera` | comune | Scelta dall'utente: oltre questo spostamento un cambio d'orario va segnalato come "da concordare" |
| `servizi[].regole` | no | comune | Regole proprie del servizio, se diverse dal contratto: `posti_minimi`, `posti_massimi`, `durata_massima_a_bordo_minuti`, `categoria_riconosciuta` |
| `servizi[].regole.pedana_obbligatoria` | no | particolare | Pedana obbligatoria per questo servizio: dice che a bordo viaggia una carrozzina |
| `servizi[].domanda.gruppi[]` | si' | | Gruppi di passeggeri (sotto) |
| `servizi[].competenze_richieste[]` | no | particolare | Versione 2. Competenze richieste a chi fa il servizio (codici dell'elenco dell'azienda del contratto). Dato particolare: una competenza come "formato per la pedana" fa capire che a bordo viaggia una carrozzina |
| `servizi[].nota` | no | comune | Note |

**Il tipo `ncc`** (versione 3, K-012). Noleggio con conducente: servizi per **gruppi formati su iniziativa del cliente o
dell'azienda** (gite, turismo, eventi, matrimoni), anche se si ripetono. Il tipo dice la **natura del servizio, non la
licenza del mezzo**: un trasporto con tragitto e frequenza fissi per una categoria di persone (es. gli operai casa-lavoro)
e' un servizio regolare specializzato anche se si fa con un mezzo da noleggio, e non e' `ncc` (va `altro`, o il tipo giusto)
[Probabile: Reg. (CE) n. 1073/2009 art. 2, stesse definizioni del Reg. (CE) n. 12/98 art. 2]. La differenza conta per le
leggi sugli orari: per il Reg. 561/2006 un servizio NCC e' un servizio occasionale, mai escluso (norme del demo), mentre una
navetta di operai scritta `ncc` diventerebbe un servizio occasionale certo. Una gita scolastica e' NCC anche se porta
studenti: chi sale non cambia il tipo (categoria della corsa, sotto "Corsa"). Nelle versioni 1 e 2 il tipo non c'e': un
servizio `altro` resta `altro`, mai trasformato in `ncc` dal programma.

**Categoria della corsa** (ricavata, mai dichiarata; dato particolare). `linea`, `disabili` (handicap), `scolastico`, `ncc`,
`promiscuo` (fino al K-011 "ibrido") o `altro`:
- piu' servizi sulla stessa corsa: promiscuo, anche se uno e' NCC;
- un solo servizio NCC: NCC, qualunque siano i gruppi (studenti, disabili, verso una scuola o un centro);
- altrimenti dai gruppi che salgono e scendono (o da quelli del servizio, o dal tipo dichiarato): gruppi di tipi diversi
  o studenti verso un centro = promiscuo; un tipo solo = linea, disabili, scolastico o altro.
Una corsa senza servizi (un errore, K-CTR-034) prende i servizi dei suoi gruppi solo per riconoscere l'NCC: i gruppi di un
solo servizio NCC = NCC, di piu' servizi con un NCC = promiscuo.

Gruppo di passeggeri (`servizi[].domanda.gruppi[]`):

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `codice` | si' | comune | Codice del gruppo (unico in tutto il pacchetto) |
| `nome` | no | comune | Nome (es. "Medie, andata") |
| `tipo` | si' | particolare | `studenti`, `disabili`, `utenti_linea`, `adulti_autorizzati`, `altro` (con `tipo_descrizione`) |
| `tipo_descrizione` | con `altro` | particolare | Descrizione del tipo |
| `verso` | si' | comune | `andata` (verso la scuola o il centro) oppure `ritorno` |
| `scuola` | no | comune | Scuola o centro di arrivo (andata) o di partenza (ritorno): da qui il grado |
| `finestra` | no | comune | `dalle`, `alle`: finestra oraria di arrivo (andata) o di partenza (ritorno) |
| `conteggi` | si' | comune | `esatti`, `stimati` oppure `non_noti` (es. gli utenti di una linea): con `non_noti` le fermate non hanno numeri |
| `fermate[]` | si' | comune | Fermate di salita e di discesa: `luogo`, `salgono`, `scendono` |
| `fermate[].carrozzine` | no | particolare | Persone che salgono a questa fermata e viaggiano **sedute sulla carrozzina** (servono pedana e posto carrozzina). Chi viaggia su un posto normale con la carrozzina piegata non conta qui, ma tra gli ausili piegati |
| `fermate[].ausili_piegati` | no | particolare | Ausili piegati da caricare a questa fermata per chi sale qui: carrozzine piegate, deambulatori e altri ausili dichiarati |
| `assistente_richiesto` | no | particolare | Il gruppo chiede l'assistente a bordo (obbligatorio se un passeggero del gruppo ne ha bisogno: errore K-CTR-047) |
| `passeggeri[]` | no | | Passeggeri uno per uno, solo nel livello `completo` (sotto) |

Passeggero (`passeggeri[]`):

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `codice` | si' | comune | Codice del passeggero (unico in tutto il pacchetto) |
| `nome` | no | personale | Nome e cognome |
| `casa` | no | personale | `indirizzo`, `comune`, `coordinate` |
| `stato.tipo` | si' | comune | `attivo`, `sospeso` (posto libero: non viaggia) oppure `sbloccato` dal committente |
| `stato.riferimento` | con `sbloccato` | comune | Riferimento dello sblocco (es. la mail del committente) |
| `salita`, `discesa` | no | comune | `luogo` e `assegnazione`: `proposta` dal programma oppure `bloccata` (scelta a mano: un calcolo futuro non la cambia finche' qualcuno non toglie il blocco) |
| `motivo` | no | personale | Motivo scritto a mano (es. perche' e' stato spostato di fermata) |
| `esigenze.carrozzina` | no | particolare | `sulla_carrozzina` (viaggia seduto sulla carrozzina: servono pedana e posto carrozzina) oppure `piegata` (viaggia su un posto normale; la carrozzina piegata e' un ausilio da caricare) |
| `esigenze.ausili_piegati` | no | particolare | Quanti altri ausili dichiarati viaggiano piegati (es. un deambulatore), oltre alla carrozzina piegata |
| `esigenze.assistente` | no | particolare | `true` = ha bisogno dell'assistente a bordo |
| `esigenze.altro` | no | particolare | Altre esigenze (testo) |

I conteggi del gruppo e i passeggeri uno per uno devono coincidere (errore K-CTR-031): le carrozzine di una
fermata sono i passeggeri che salgono li' con `carrozzina: sulla_carrozzina`; gli ausili piegati sono le
carrozzine `piegata` piu' gli `ausili_piegati` dei passeggeri che salgono li'. La domanda e le persone
caricate dalle corse possono differire (avviso K-CTR-042). Un passeggino non si dichiara e non si pianifica.

```json
{
  "codice": "G-AM-MEDIE",
  "nome": "Medie, andata",
  "tipo": "studenti",
  "verso": "andata",
  "scuola": "SC-MEDIE",
  "finestra": { "dalle": "07:30", "alle": "07:55" },
  "conteggi": "esatti",
  "fermate": [
    { "luogo": "F-PRADELLO", "salgono": 9 },
    { "luogo": "F-CASTELROVO", "salgono": 8 },
    { "luogo": "F-MONTEBRINO", "salgono": 7 },
    { "luogo": "L-MEDIE", "scendono": 24 }
  ]
}
```

### corse.json

La **corsa** e' il viaggio vero di un mezzo. Puo' soddisfare piu' servizi, anche di contratti diversi:
allora e' una corsa promiscua (es. medie del consorzio + superiori a supporto di una linea). Schema:
[docs/schema/corse.v1.schema.json](schema/corse.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `corse[].codice` | si' | comune | Codice della corsa |
| `corse[].nome` | no | comune | Nome |
| `corse[].codici_esterni[]` | no | comune | Es. sistema "Trasporti Provinciali": linea "B634", corsa "533"; turno "1234" |
| `corse[].servizi[]` | si' | comune | Servizi che la corsa soddisfa (piu' di uno = promiscua; nessuno: avviso K-CTR-040) |
| `corse[].calendario` | si' | comune | Calendario |
| `corse[].passaggi[]` | si' | comune | Sequenza dei passaggi (sotto) |
| `corse[].km_totali` | no | comune | `pieni`, `vuoti`: totali come li scrive il committente sulla sua scheda (un dato, non un calcolo) |
| `corse[].coincidenze[]` | no | comune | `luogo`, `servizio_esterno` (es. "treno FTM n. 3"), `orario`, `tipo`: `aspettare_arrivo` (la corsa parte dopo l'arrivo indicato) oppure `garantire_partenza` (la corsa arriva prima della partenza indicata) |
| `corse[].testo_esposto` | no | comune | Testo da esporre sul mezzo |
| `corse[].nota` | no | comune | Note |
| `passaggi[].luogo` | si' | comune | Luogo |
| `passaggi[].orario` | si' | comune | Arrivo nel luogo |
| `passaggi[].partenza` | no | comune | Partenza, se il mezzo aspetta |
| `passaggi[].transito` | no | comune | `true` = passa senza fermarsi (allora niente persone, assistente, trasbordi) |
| `passaggi[].persone[]` | no | comune | Persone di ogni `gruppo` che `salgono` e `scendono` (come "St. Car." e "St. Scar." sulle schede) |
| `passaggi[].assistente` | no | comune | `sale` oppure `scende`: movimento dell'assistente (persona del personale, non un passeggero) |
| `passaggi[].trasbordi[]` | no | comune | Meta' di un trasbordo: `codice` (uguale sulle due meta'), `azione` (`consegna` o `ricezione`), `gruppo`, `persone`, `passeggero` (facoltativo; con il passeggero, `persone` vale 1) |
| `passaggi[].km` | no | comune | `pieni`, `vuoti`: km dal passaggio precedente, come li scrive il committente |
| `passaggi[].nota` | no | comune | Note |

**Trasbordo**: una consegna su una corsa e una ricezione su un'altra corsa, nello stesso punto di
trasbordo autorizzato, per lo stesso gruppo o passeggero. La corsa che riceve deve essere sul posto prima
di quella che consegna, con il margine del manifesto: l'attesa del passeggero non si pianifica. Il trasbordo
tra corse di aziende diverse e' ammesso. Controlli: K-CTR-023 ... K-CTR-027.

```json
{
  "codice": "C-RM-1",
  "nome": "Ritorno mezzogiorno superiori e medie",
  "servizi": ["S-SUP-RM", "S-SCU-RM"],
  "calendario": "CAL-SCUOLA-LV",
  "passaggi": [
    { "luogo": "L-SUP", "orario": "11:58", "persone": [{ "gruppo": "G-RM-SUP", "salgono": 5 }] },
    { "luogo": "L-MEDIE", "orario": "12:05", "persone": [{ "gruppo": "G-RM-MEDIE", "salgono": 11 }], "km": { "pieni": 0.6 } },
    { "luogo": "F-VALFONDA-PIAZZA", "orario": "12:07", "transito": true, "km": { "pieni": 0.5 } },
    { "luogo": "F-CASTELROVO", "orario": "12:19",
      "persone": [{ "gruppo": "G-RM-MEDIE", "scendono": 4 }, { "gruppo": "G-RM-SUP", "scendono": 2 }],
      "km": { "pieni": 7.6 } }
  ]
}
```

### mezzi.json

Schemi: [docs/schema/mezzi.v3.schema.json](schema/mezzi.v3.schema.json) (versione attuale, K-012: il tipo di servizio `ncc`
nel vincolo di destinazione; per passare alla 3 si cambia solo `"schema_versione"`),
[docs/schema/mezzi.v2.schema.json](schema/mezzi.v2.schema.json) (versione 2, ancora letta) e
[docs/schema/mezzi.v1.schema.json](schema/mezzi.v1.schema.json) (versione 1, ancora letta: e' la versione 2 senza
`uso`, `autorizzazioni_uso`, `vincolo_destinazione`, `scorta` e `prestiti`). I posti non cambiano con l'eta' dei
passeggeri.

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `mezzi[].codice` | si' | comune | Codice del mezzo |
| `mezzi[].azienda` | si' | comune | Azienda |
| `mezzi[].targa` | si' | comune | Targa |
| `mezzi[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `mezzi[].categoria` | si' | comune | `M1`, `M2`, `M3` |
| `mezzi[].posti` | si' | comune | Posti per i passeggeri, **conducente escluso** (sulla carta di circolazione il campo S.1 lo comprende) |
| `mezzi[].posti_in_piedi` | no | comune | Solo se previsti dal libretto (campo S.2) |
| `mezzi[].carrozzine` | no | comune | Carrozzine trasportabili al massimo |
| `mezzi[].posti_persi_per_carrozzina` | no | comune | Posti a sedere persi per ogni carrozzina caricata |
| `mezzi[].pedana`, `tempo_pedana_secondi` | no | comune | Pedana o sollevatore e il tempo per azionarla a ogni fermata |
| `mezzi[].ausili_bagagliaio` | no | comune | Quanti ausili piegati entrano nel bagagliaio o nel baule (0 o assente = niente bagagliaio) |
| `mezzi[].ausili_tra_i_sedili.posti_occupati` | no | comune | Presente = l'azienda ammette un ausilio piegato tra i sedili, legato e bloccato con cinghie o ancoraggi; ogni ausilio occupa questi posti (es. 2). Assente = non ammesso |
| `mezzi[].rimorchio` | no | comune | Rimorchio |
| `mezzi[].misure` | no | comune | `lunghezza_m`, `larghezza_m`, `altezza_m`, `massa_t`, `sbalzo_posteriore_m` |
| `mezzi[].alimentazione` | no | comune | `tipo` (`gasolio`, `benzina`, `metano`, `gpl`, `ibrido`, `elettrico`, `idrogeno`, `altro` con `descrizione`); per gli elettrici `batteria_kwh`, `consumo_kwh_km`, `carica_minima_percento` |
| `mezzi[].costo_fisso_giornaliero` | no | comune | Assicurazione, bollo, ammortamento... per giorno (senza: avviso K-CTR-044) |
| `mezzi[].costo_al_km` | no | comune | Costo per km |
| `mezzi[].ipotetico` | no | comune | `true` = mezzo non ancora in flotta (scenari) |
| `mezzi[].uso` | no | comune | Versione 2. Uso scritto sulla carta di circolazione: codice dell'elenco `usi` delle norme dell'azienda (senza: avviso K-CTR-051 se le norme lo usano) |
| `mezzi[].autorizzazioni_uso[]` | no | comune | Versione 2. Autorizzazioni a un uso in piu' (la "distrazione"): `uso`, `ente`, `riferimento`, `dal`, `al`, `prescrizioni`. Per singolo mezzo e con le date; chi la rilascia e' un dato (cambia con il verso e il territorio) |
| `mezzi[].vincolo_destinazione` | no | comune | Versione 2. Il mezzo fa solo certi servizi: `ammessi` (`tipi_servizio` e/o `contratti`), `fino_al`, `fonte`, `nota` (es. comprato con contributi per le linee: niente noleggio) |
| `mezzi[].scorta` | no | comune | Versione 2. `true` = mezzo di scorta: se nessun turno lo usa non e' un avviso |
| `mezzi[].prestiti[]` | no | comune | Versione 2. Prestiti ad altre aziende (subaffidamento, consorzio, noleggio): `azienda`, `accordo`, `dal`, `al`. Senza prestito il mezzo non entra nei turni di un'altra azienda |
| `mezzi[].nota` | no | comune | Note |

**Uso del mezzo e distrazione.** La carta di circolazione dice l'uso del mezzo (in Italia Codice della strada
art. 82: uso proprio oppure uso di terzi, che comprende noleggio con conducente e servizio di linea). Un mezzo fa solo
i servizi per cui e' omologato; per un altro uso serve un'autorizzazione da tenere a bordo (art. 87, comma 4;
circolare del Ministero dei trasporti n. 27690 del 27/11/2013): e' la distrazione, per singolo mezzo e con le date.
Quali usi servono per ogni tipo di servizio lo dicono le norme del paese e il regolamento del territorio, mai il
programma.

**Carrozzine e ausili piegati.** Chi viaggia **seduto sulla carrozzina** ha bisogno della pedana e di un posto
carrozzina, e ogni carrozzina toglie i posti a sedere indicati. Chi viaggia su un posto normale con la
carrozzina **piegata** non ha bisogno della pedana: la carrozzina piegata e' un **ausilio piegato**, come un
deambulatore. Un ausilio piegato va prima nel bagagliaio (`ausili_bagagliaio`); se il bagagliaio non c'e' o
e' pieno, va tra i sedili, legato e bloccato, solo se il mezzo lo ammette (`ausili_tra_i_sedili`) e se i posti
che occupa sono liberi; altrimenti il passeggero con quell'ausilio non e' ammesso su quel mezzo.
"Tra i sedili" e' una **scelta facoltativa dell'azienda**, che ne risponde: senza il campo il programma non
la propone mai. Semplificazione: **si contano gli ausili, non il loro volume** (un deambulatore conta come una
carrozzina piegata).

### rimesse.json

Rimesse e ricoveri dove i mezzi passano la notte. Schema: [docs/schema/rimesse.v1.schema.json](schema/rimesse.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `rimesse[].codice` | si' | comune | Codice |
| `rimesse[].luogo` | si' | comune | Luogo (di tipo `rimessa` o `ricovero_esterno`, anche privato) |
| `rimesse[].azienda` | si' | comune | Azienda |
| `rimesse[].ricarica` | si' | comune | Ricarica per i mezzi elettrici |
| `rimesse[].posti_mezzi` | no | comune | Quanti mezzi ci stanno |
| `rimesse[].nota` | no | comune | Note |

### tempi_sosta.json

Tempi di sosta alle fermate, facoltativi finche' non confermati (senza: avviso K-CTR-036). Il tempo della
pedana resta sul mezzo. Schema: [docs/schema/tempi_sosta.v1.schema.json](schema/tempi_sosta.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `tempi.fissa_per_fermata_secondi` | no | comune | Parte fissa per ogni fermata |
| `tempi.per_passeggero[]` | no | comune | Per `tipo` (`studente`, `infanzia`, `disabile`): `salita_secondi` e `discesa_secondi` per passeggero |
| `tempi.scarico_a_scuola_secondi` | no | comune | Tempo di scarico a scuola |
| `tempi.nota` | no | comune | Note |

### persone.json

Le persone delle aziende: autisti, assistenti a bordo o tutti e due. Schemi: versione 2 (K-011)
[docs/schema/persone.v2.schema.json](schema/persone.v2.schema.json); versione 1, che si legge ancora,
[docs/schema/persone.v1.schema.json](schema/persone.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `persone[].codice` | si' | comune | Codice della persona: compare al posto del nome nelle uscite di base |
| `persone[].azienda` | si' | comune | Azienda della persona |
| `persone[].ruoli[]` | si' | personale | `autista`, `assistente` (o tutti e due) |
| `persone[].nome` | no | personale | Nome e cognome (non nel livello "solo numeri") |
| `persone[].casa` | no | personale | `indirizzo`, `comune`, `coordinate` (non nel livello "solo numeri"): servira' per i ricoveri vicini a casa e per le pause |
| `persone[].presa_servizio` | no | comune | Rimessa o luogo abituale di presa servizio (codice di un luogo) |
| `persone[].mezzo_abituale` | no | comune | Mezzo abituale (abbinamento autista-mezzo, turni con il "mezzo abituale") |
| `persone[].ipotetica` | no | comune | `true` = persona non ancora assunta (scenari) |
| `persone[].rapporto` | si' | personale | `tipo`: `titolare`, `collaboratore_familiare`, `socio`, `dipendente_tempo_pieno`, `dipendente_part_time`, `a_chiamata`, `tempo_determinato`, `stagionale`, `autonomo`, `volontario` (es. accompagnatori autorizzati dal committente), `altro` con `descrizione` |
| `persone[].contratto_collettivo` | no | personale | `codice` (es. il codice CNEL) e `nome` |
| `persone[].ore[]` | no | personale | Ore `previste` e `massime` per `giorno`, `settimana`, `mese` (ore con decimali), con `dal` e `al` facoltativi |
| `persone[].disponibilita[]` | no | personale | Giorni e fasce di disponibilita' (rigida): `giorni`, `dalle`, `alle`, con `dal` e `al` facoltativi |
| `persone[].costo_orario[]` | no | personale | `valore` nella valuta del pacchetto, con `dal` e `al` facoltativi |
| `persone[].abilitazioni[]` | no | personale | `tipo` (codice delle norme dell'azienda), `scadenza`, `riferimento` |
| `persone[].autorizzazioni[]` | no | personale | Autorizzazioni del committente: `committente`, `riferimento`, `scadenza` (es. il cartellino per i servizi speciali) |
| `persone[].competenze[]` | no | personale | Competenze (codici dell'elenco dell'azienda della persona) |
| `persone[].preferenze[]` | no | personale | Preferenze (dati morbidi, sotto), con `dal` e `al` facoltativi |
| `persone[].prestiti[]` | no | comune | Prestiti ad altre aziende: `azienda`, `accordo`, `dal`, `al` |
| `persone[].scelte_orari[]` | no | personale | Versione 2: scelte sugli orari della persona, come quelle dell'azienda: vincono su quelle dello stesso tipo e periodo |
| `persone[].rientro` | no | personale | Versione 2: chiave del rientro della persona; ogni campo scritto vince su quello dell'azienda |
| `persone[].giornate_a_chiamata` | no | personale | Versione 2: `giornate` di lavoro con lo stesso datore prima del periodo del pacchetto, `contate_dal` un giorno (rapporto a chiamata) |
| `persone[].nota` | no | personale | Note (non nel livello "solo numeri"; mai cause di assenza o dati sulla salute) |

**Disponibilita' e preferenze.** La disponibilita' e' rigida: se per un giorno vale almeno una riga (per le date),
la persona e' disponibile solo dentro le fasce di quel giorno della settimana; un turno fuori e' un fatto che la
rende non disponibile. Senza righe per quel giorno, nessun limite. Le preferenze sono morbide: un turno che non le
rispetta e' un avviso, mai un errore. Tipi: `giorni_liberi` (`giorni`), `fascia_oraria` (`dalle`, `alle`),
`evita_turni` (`turni`), `pausa_massima` (`minuti`: nessuna pausa piu' lunga dentro un turno, es. "niente turni
spezzati" con la soglia scelta dall'utente), `altro` (`descrizione`: mostrata, non controllata).

**Ore e contratto collettivo** (K-011): le ore `massime` sono limiti rigidi, le `previste` si mostrano con la
differenza; che cosa conta come "ora" lo dice il regime che vale quel giorno (sopra, "Orari di lavoro"). Il contratto
collettivo si collega ai regimi delle norme con il suo `codice`. Il costo orario resta solo un dato (servira' al calcolo).

Una persona nel livello "completo" (nel livello "solo numeri" mancano `nome`, `casa` e `nota`):

```json
{
  "codice": "P-C",
  "azienda": "ALFA",
  "ruoli": ["autista"],
  "nome": "Verdi Carlo",
  "presa_servizio": "L-RIMESSA-ALFA",
  "rapporto": { "tipo": "titolare" },
  "abilitazioni": [{ "tipo": "B" }, { "tipo": "D", "scadenza": "2030-01-15" }, { "tipo": "CQC-P", "scadenza": "2027-06-30" }],
  "autorizzazioni": [{ "committente": "CVA", "riferimento": "Cartellino n. 9 (inventato)", "scadenza": "2027-08-31" }],
  "competenze": ["PEDANA", "LINEA-B610", "ELETTRICO"],
  "prestiti": [{ "azienda": "BETA", "accordo": "Accordo consortile n. 2/2026 (inventato)", "dal": "2026-09-01", "al": "2027-08-31" }]
}
```

### assenze.json

Assenze programmate delle persone. **Solo il tipo, mai la causa**: il formato non ha un campo per scriverla. Schema:
[docs/schema/assenze.v1.schema.json](schema/assenze.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `assenze[].persona` | si' | comune | Codice della persona |
| `assenze[].tipo` | si' | personale | Tipo dall'elenco dell'azienda della persona (es. ferie, permesso, formazione) |
| `assenze[].dal`, `al` | si' | personale | Primo e ultimo giorno |
| `assenze[].dalle`, `alle` | no | personale | Solo una parte di ogni giorno del periodo (vanno scritti insieme) |

```json
{ "persona": "P-A", "tipo": "FERIE", "dal": "2026-10-22", "al": "2026-10-23" }
```

Che una persona abbia un'assenza e' gia' un dato personale: nello schema il file intero e ogni assenza sono marcati
personali, e ogni assenza indica la persona a cui appartiene (`x-kzero-titolare`). Per questo, di base, la posizione di
un problema su un'assenza e' la persona in `persone.json`, non `assenze.json` (docs/controlli.md).

### fermi.json

Fermi programmati dei mezzi. Schema: [docs/schema/fermi.v1.schema.json](schema/fermi.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `fermi[].mezzo` | si' | comune | Codice del mezzo |
| `fermi[].tipo` | si' | comune | Tipo dall'elenco dell'azienda del mezzo (es. revisione, manutenzione) |
| `fermi[].dal`, `al` | si' | comune | Primo e ultimo giorno |
| `fermi[].dalle`, `alle` | no | comune | Solo una parte di ogni giorno del periodo (vanno scritti insieme) |
| `fermi[].nota` | no | comune | Note |

```json
{ "mezzo": "ALFA-01", "tipo": "REVISIONE", "dal": "2026-10-19", "al": "2026-10-20", "nota": "Revisione annuale in officina." }
```

### turni.json

Il **turno del giorno** e' l'unita' di base del lavoro di una persona in un giorno: un elenco ordinato di corse
(intere o a pezzi) e di attivita' senza passeggeri, con il mezzo, il luogo di inizio e di fine e il calendario dei
giorni in cui vale. Il turno non dice chi lo fa: lo dicono le rotazioni, i servizi fissi e le assegnazioni del
singolo giorno (sotto). La **giornata della persona** (i turni che fa in un giorno) e la **giornata del mezzo** (i
turni che lo usano) sono due viste degli stessi turni: non si scrivono due volte. Schema:
[docs/schema/turni.v1.schema.json](schema/turni.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `turni[].codice` | si' | comune | Codice del turno |
| `turni[].nome` | no | comune | Nome dato dall'azienda (es. "Primo", "Festivo 3"): solo un nome |
| `turni[].azienda` | si' | comune | Azienda del turno |
| `turni[].ruolo` | si' | comune | `autista`, `assistente` (a bordo) o `secondo_autista` (viaggi lunghi, affiancamento di un autista nuovo) |
| `turni[].calendario` | si' | comune | Giorni in cui vale il turno (calendari.json) |
| `turni[].mezzo` | si' | comune | `modo`: `preciso` (con il `codice` del mezzo), `abituale` (il mezzo abituale di chi fa il turno), `da_assegnare`, `nessuno` (riserva, officina, ufficio; turni da assistente e da secondo autista) |
| `turni[].inizio`, `fine` | si' | comune | Luogo di inizio e di fine (rimessa, ricovero esterno o punto di sosta) |
| `turni[].elementi[]` | si' | comune | Corse e attivita' in ordine di orario (sotto) |
| `turni[].competenze_richieste[]` | no | particolare | Competenze richieste a chi fa il turno (elenco dell'azienda): possono far capire chi viaggia |
| `turni[].nota` | no | comune | Note |

Un **elemento** e' una corsa oppure un'attivita':

| Campo | Obbl. | Significato |
|---|---|---|
| `corsa` | si' (corsa) | Codice della corsa |
| `da`, `a` | no | Solo un pezzo della corsa (cambio dell'autista lungo la corsa): `luogo` di un passaggio e, se la corsa ci passa piu' volte, `orario`. Senza `da`: dall'inizio della corsa; senza `a`: fino alla fine |
| `assistente` | no | Per le corse con l'assistente a bordo, chi lo fornisce: `azienda` (una persona con il suo turno da assistente) o `committente` (senza nome). Assente = come dice il contratto |
| `attivita` | si' (attivita') | Tipo di attivita' dall'elenco dell'azienda del turno (es. presa servizio, pulizia, rifornimento, ricarica, attesa, pausa, trasferimento a vuoto, riserva, reperibilita') |
| `luogo`, `verso` | si' / no | Luogo dell'attivita'; `verso` = luogo di arrivo per le attivita' che spostano il mezzo o la persona |
| `dalle`, `alle` | si' | Orari dell'attivita' (anche oltre le 24:00) |
| `km_vuoti` | no | Km a vuoto dell'attivita', come li scrive chi pianifica (un dato, non un calcolo) |
| `calendario` | no | Giorni in cui c'e' l'attivita' (dentro quelli del turno); assente = tutti i giorni del turno |
| `nota` | no | Note |

**Inizio e fine del turno** non si scrivono: si ricavano dalla prima e dall'ultima corsa o attivita' di ogni giorno
(in un giorno valgono solo le corse e le attivita' il cui calendario vale quel giorno). Un turno che finisce dopo
la mezzanotte resta del giorno in cui comincia ("24:12"). **Turno spezzato**: due parti con una pausa in mezzo
(l'attivita' "pausa" e' facoltativa). **Turno notturno**: orari oltre le 24:00. **Cambio di mezzo** a meta'
giornata: due turni (uno per mezzo), fatti dalla stessa persona. **Cambio dell'autista a meta' corsa**: la corsa
compare in due turni, ognuno con il suo pezzo; il luogo del cambio e' il **punto di cambio**. Se l'attivita' del
turno impegna il mezzo lo dice l'elenco dell'azienda (`usa_il_mezzo` in aziende.json): una pausa o una riserva non lo
impegnano.

**Turni da assistente e da secondo autista** elencano le corse in cui la persona e' a bordo; il mezzo e' quello del
turno dell'autista che fa quelle corse.

Il secondo turno del demo: va a piedi al punto di cambio, prende l'autobus a meta' della corsa C-LIN-549 e finisce in
rimessa (alcune attivita' tolte per brevita'):

```json
{
  "codice": "T-SECONDO",
  "nome": "Secondo",
  "azienda": "ALFA",
  "ruolo": "autista",
  "calendario": "CAL-LV-FER",
  "mezzo": { "modo": "preciso", "codice": "ALFA-05" },
  "inizio": "L-RIMESSA-ALFA",
  "fine": "L-RIMESSA-ALFA",
  "elementi": [
    { "attivita": "A-PIEDI", "luogo": "L-RIMESSA-ALFA", "verso": "F-VALFONDA-PIAZZA", "dalle": "16:50", "alle": "17:00" },
    { "corsa": "C-LIN-549", "da": { "luogo": "F-VALFONDA-PIAZZA" } },
    { "attivita": "VUOTO", "luogo": "F-STAZIONE", "verso": "L-RIMESSA-ALFA", "dalle": "17:13", "alle": "17:21", "km_vuoti": 1.0 },
    { "corsa": "C-LIN-553" },
    { "attivita": "FINE", "luogo": "L-RIMESSA-ALFA", "dalle": "20:22", "alle": "20:30" }
  ]
}
```

### rotazioni.json

Una **rotazione** e' un ciclo di passi che si ripete: per ogni passo e ogni persona, i turni che fa oppure
"riposo". Ogni azienda fa la sua: nessuna lunghezza e nessuna forma e' fissa nel programma (cicli di 3, 8, 14
giorni; passi di un mese per le aziende che cambiano a mesi). Piu' rotazioni possono valere nello stesso anno (es.
invernale ed estiva) e nello stesso periodo per gruppi diversi di persone. Schema:
[docs/schema/rotazioni.v1.schema.json](schema/rotazioni.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `rotazioni[].codice` | si' | comune | Codice della rotazione |
| `rotazioni[].nome` | no | comune | Nome (es. "Rotazione invernale") |
| `rotazioni[].passo` | si' | comune | `giorno` oppure `mese` (mese di calendario) |
| `rotazioni[].lunghezza` | si' | comune | Quanti passi ha il ciclo (da 1 a 1000) |
| `rotazioni[].partenza` | si' | comune | Giorno del primo passo (con i passi mensili: il primo giorno di un mese) |
| `rotazioni[].dal`, `al` | no | comune | Date in cui vale la rotazione; assenti = dalla partenza alla fine del periodo del pacchetto |
| `rotazioni[].righe[]` | si' | comune | Una riga per persona: `persona` e `passi` (uno per ogni passo del ciclo) |
| `rotazioni[].nota` | no | personale | Note (possono parlare delle persone) |

Un **passo** e' `"riposo"` oppure l'elenco dei turni della persona in quel passo, es. `["T-PRIMO", "T-SABATO"]`:
in un giorno valgono quelli il cui calendario vale quel giorno (cosi' un passo puo' avere il turno del lunedi'-venerdi'
e quello del sabato insieme).

**Come si conta.** Il passo di un giorno si conta dalla `partenza`: passo = (giorni dalla partenza) diviso la
lunghezza, resto. Il ciclo **non si ferma** nelle vacanze ne' nei giorni senza turni: un giorno in cui il turno del
passo non vale (festa, vacanza della scuola) quella persona semplicemente non lavora, e il giorno dopo il ciclo va
avanti. Si conta anche prima della partenza (con `dal` precedente). Con i passi mensili, il passo e' il numero di mesi
di calendario dalla partenza. La regola dell'azienda ALP, con la tabella di 14 giorni, e' scritta in REGOLE.md.

La linea 7 su 7 del demo: tre autisti, ciclo di 3 giorni, il riposo scorre; il passo con due turni vale nei giorni
feriali per il primo e nei festivi per l'altro, secondo i loro calendari:

```json
{
  "codice": "ROT-BETA",
  "nome": "Linea 7 su 7, ciclo di 3 giorni",
  "passo": "giorno",
  "lunghezza": 3,
  "partenza": "2026-09-01",
  "righe": [
    { "persona": "P-B1", "passi": [["T-B-PRIMO", "T-B-FESTIVO"], "riposo", ["T-B-ASILO", "T-B-SERA"]] },
    { "persona": "P-B2", "passi": [["T-B-ASILO", "T-B-SERA"], ["T-B-PRIMO", "T-B-FESTIVO"], "riposo"] },
    { "persona": "P-B3", "passi": ["riposo", ["T-B-ASILO", "T-B-SERA"], ["T-B-PRIMO", "T-B-FESTIVO"]] }
  ]
}
```

### servizi_fissi.json

Un **servizio fisso** e' un turno assegnato sempre alla stessa persona nei giorni di un calendario (es. il
mercoledi'). Vale piu' della rotazione e meno di un'assegnazione del singolo giorno. Se un'azienda tiene fisse solo
alcune corse, le mette in un turno a parte. Schema:
[docs/schema/servizi_fissi.v1.schema.json](schema/servizi_fissi.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `servizi_fissi[].codice` | si' | comune | Codice |
| `servizi_fissi[].turno` | si' | comune | Turno |
| `servizi_fissi[].persona` | si' | comune | Persona |
| `servizi_fissi[].calendario` | si' | comune | Giorni in cui il turno e' di questa persona; gli altri giorni il turno segue la rotazione |
| `servizi_fissi[].nota` | no | personale | Note |

```json
{ "codice": "SF-MER", "turno": "T-SECONDO", "persona": "P-C", "calendario": "CAL-MER-FER" }
```

### assegnazioni.json

Un'**assegnazione del singolo giorno** dice chi fa un turno in una data, con quale mezzo, o tutti e due:
sostituzioni, scambi tra colleghi, un mezzo al posto di quello in revisione, lavori occasionali (una gita, un
noleggio). Vale piu' dei servizi fissi e delle rotazioni. Il programma non le inventa: le registra se le scrive
l'utente. Schema: [docs/schema/assegnazioni.v1.schema.json](schema/assegnazioni.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `assegnazioni[].data` | si' | comune | Il giorno |
| `assegnazioni[].turno` | si' | comune | Il turno |
| `assegnazioni[].persona` | no | comune | Chi fa il turno quel giorno |
| `assegnazioni[].mezzo` | no | comune | Con quale mezzo quel giorno (almeno uno tra `persona` e `mezzo`) |
| `assegnazioni[].nota` | no | personale | Note (mai la causa di un'assenza) |

Una sostituzione e un mezzo al posto di quello in revisione:

```json
[
  { "data": "2026-10-22", "turno": "T-SECONDO", "persona": "P-C" },
  { "data": "2026-10-19", "turno": "T-SCUOLA-1", "mezzo": "ALFA-07" }
]
```

**Chi fa il turno in un giorno**: l'assegnazione del singolo giorno, se c'e'; altrimenti il servizio fisso, se il
suo calendario vale quel giorno; altrimenti la rotazione. Vince il modo piu' preciso, turno per turno. Se nessun
modo da' una persona, il turno quel giorno e' **scoperto** (K-CTR-077): il programma non inventa assegnazioni.
Il **mezzo** in un giorno: quello dell'assegnazione del singolo giorno, se c'e'; altrimenti quello del turno
(preciso, oppure il mezzo abituale di chi fa il turno). Se la persona prevista quel giorno non puo' (assente, fuori
dalla disponibilita', abilitazione scaduta, prestito non valido), il turno e' scoperto; se il mezzo non puo' (fermo,
autorizzazione all'uso scaduta, prestito non valido), il turno e' senza mezzo. Il rimedio e' un'assegnazione del
singolo giorno (controlli.md, "Controlli sui turni").

### blocchi.json

Un **blocco** fissa delle corse su un mezzo, con le date e la forza: e' un vincolo per il calcolo futuro. Oggi si
controlla che le corse bloccate siano fatte con il loro mezzo (K-CTR-074) e che due blocchi non si contraddicano
(K-CTR-075). Per le persone l'equivalente e' il servizio fisso. Schema:
[docs/schema/blocchi.v1.schema.json](schema/blocchi.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `blocchi[].codice` | si' | comune | Codice |
| `blocchi[].mezzo` | si' | comune | Mezzo |
| `blocchi[].corse[]` | si' | comune | Corse del blocco (scelte per servizio, per fascia oraria o una per una: alla fine sono sempre corse) |
| `blocchi[].dal`, `al` | no | comune | Date del blocco; assenti = tutto il periodo |
| `blocchi[].forza` | si' | comune | `mezzo_fisso` (le corse restano a questo mezzo; un calcolo futuro potra' riorganizzare i giri con quel mezzo) oppure `tutto_fisso` (mezzo, corse e orari restano come sono) |
| `blocchi[].nota` | no | comune | Note |

```json
{ "codice": "B-LINEA", "mezzo": "ALFA-05", "corse": ["C-LIN-533", "C-LIN-560"], "forza": "tutto_fisso" }
```

## Ricerca dei luoghi per nome o codice esterno

Una fermata si trova per **nome ufficiale**, **nome alternativo** oppure **codice esterno**
(sistema + tipo + codice). Prima del confronto i testi passano per una **normalizzazione fissa**:

1. le lettere latine con segni (accenti, dieresi, cediglie, barre...) diventano lettere semplici maiuscole,
   secondo la tabella qui sotto (es. "ò" diventa "O", "ß" diventa "SS", "ł" diventa "L", "æ" diventa "AE");
2. le lettere a-z diventano maiuscole; i segni staccati (accento scritto come carattere a parte, da U+0300
   a U+036F) si tolgono;
3. punteggiatura, simboli e spazi di ogni tipo diventano uno spazio; gli spazi doppi si riducono a uno,
   quelli all'inizio e alla fine si tolgono. Sono punteggiatura e simboli: tutti i caratteri ASCII che non
   sono lettere o cifre; i segni di Latin-1 da U+0080 a U+00BF (es. «, », °, ª, º, µ) tranne ², ³ e ¹
   (che diventano cifre, vedi tabella); × e ÷; la punteggiatura generale da U+2000 a U+206F (trattini,
   virgolette e apostrofi tipografici, spazi speciali); il segno invisibile U+FEFF;
4. tutto il resto resta com'e': cifre, lettere di altri alfabeti (greco, cirillico...) con le loro maiuscole
   e minuscole, byte che non sono UTF-8.

Esempi: "Pradello, Piazza", "pradello piazza" e "PRADELLO - piazza." diventano tutti `PRADELLO PIAZZA`;
"Cagnò" e "Cagno'" diventano `CAGNO`; "Straße" e "Strasse" diventano `STRASSE`; "Łódź" diventa `LODZ`.
"Pradello P.zza" diventa `PRADELLO P ZZA`: un'abbreviazione resta un nome diverso (va scritta tra i nomi
alternativi).

Esiti della ricerca: **trovato** (un solo luogo), **sconosciuto** (nessuno) oppure **ambiguo** (piu' luoghi:
tutti elencati). Nessuna somiglianza "a occhio": il nome coincide, oppure no. Due fermate con lo stesso nome
o lo stesso codice esterno dopo la normalizzazione sono un errore (K-CTR-018). I luoghi privati (case) non
si cercano per nome.

Nessuna lettera fa fallire la ricerca e due nomi diversi diventano uguali solo come scritto in questa
pagina. Limite noto: le lettere di altri alfabeti non vengono ricondotte all'alfabeto latino ("Αθήνα" e
"Athina" restano diversi) e per loro maiuscole e minuscole contano.

Tabella completa delle lettere trasformate al passo 1 (una prova controlla che coincida con il programma,
riga per riga):

| Lettera | Codice | Diventa |
|---|---|---|
| ² | U+00B2 | 2 |
| ³ | U+00B3 | 3 |
| ¹ | U+00B9 | 1 |
| À | U+00C0 | A |
| Á | U+00C1 | A |
| Â | U+00C2 | A |
| Ã | U+00C3 | A |
| Ä | U+00C4 | A |
| Å | U+00C5 | A |
| Æ | U+00C6 | AE |
| Ç | U+00C7 | C |
| È | U+00C8 | E |
| É | U+00C9 | E |
| Ê | U+00CA | E |
| Ë | U+00CB | E |
| Ì | U+00CC | I |
| Í | U+00CD | I |
| Î | U+00CE | I |
| Ï | U+00CF | I |
| Ð | U+00D0 | D |
| Ñ | U+00D1 | N |
| Ò | U+00D2 | O |
| Ó | U+00D3 | O |
| Ô | U+00D4 | O |
| Õ | U+00D5 | O |
| Ö | U+00D6 | O |
| Ø | U+00D8 | O |
| Ù | U+00D9 | U |
| Ú | U+00DA | U |
| Û | U+00DB | U |
| Ü | U+00DC | U |
| Ý | U+00DD | Y |
| Þ | U+00DE | TH |
| ß | U+00DF | SS |
| à | U+00E0 | A |
| á | U+00E1 | A |
| â | U+00E2 | A |
| ã | U+00E3 | A |
| ä | U+00E4 | A |
| å | U+00E5 | A |
| æ | U+00E6 | AE |
| ç | U+00E7 | C |
| è | U+00E8 | E |
| é | U+00E9 | E |
| ê | U+00EA | E |
| ë | U+00EB | E |
| ì | U+00EC | I |
| í | U+00ED | I |
| î | U+00EE | I |
| ï | U+00EF | I |
| ð | U+00F0 | D |
| ñ | U+00F1 | N |
| ò | U+00F2 | O |
| ó | U+00F3 | O |
| ô | U+00F4 | O |
| õ | U+00F5 | O |
| ö | U+00F6 | O |
| ø | U+00F8 | O |
| ù | U+00F9 | U |
| ú | U+00FA | U |
| û | U+00FB | U |
| ü | U+00FC | U |
| ý | U+00FD | Y |
| þ | U+00FE | TH |
| ÿ | U+00FF | Y |
| Ā | U+0100 | A |
| ā | U+0101 | A |
| Ă | U+0102 | A |
| ă | U+0103 | A |
| Ą | U+0104 | A |
| ą | U+0105 | A |
| Ć | U+0106 | C |
| ć | U+0107 | C |
| Ĉ | U+0108 | C |
| ĉ | U+0109 | C |
| Ċ | U+010A | C |
| ċ | U+010B | C |
| Č | U+010C | C |
| č | U+010D | C |
| Ď | U+010E | D |
| ď | U+010F | D |
| Đ | U+0110 | D |
| đ | U+0111 | D |
| Ē | U+0112 | E |
| ē | U+0113 | E |
| Ĕ | U+0114 | E |
| ĕ | U+0115 | E |
| Ė | U+0116 | E |
| ė | U+0117 | E |
| Ę | U+0118 | E |
| ę | U+0119 | E |
| Ě | U+011A | E |
| ě | U+011B | E |
| Ĝ | U+011C | G |
| ĝ | U+011D | G |
| Ğ | U+011E | G |
| ğ | U+011F | G |
| Ġ | U+0120 | G |
| ġ | U+0121 | G |
| Ģ | U+0122 | G |
| ģ | U+0123 | G |
| Ĥ | U+0124 | H |
| ĥ | U+0125 | H |
| Ħ | U+0126 | H |
| ħ | U+0127 | H |
| Ĩ | U+0128 | I |
| ĩ | U+0129 | I |
| Ī | U+012A | I |
| ī | U+012B | I |
| Ĭ | U+012C | I |
| ĭ | U+012D | I |
| Į | U+012E | I |
| į | U+012F | I |
| İ | U+0130 | I |
| ı | U+0131 | I |
| Ĳ | U+0132 | IJ |
| ĳ | U+0133 | IJ |
| Ĵ | U+0134 | J |
| ĵ | U+0135 | J |
| Ķ | U+0136 | K |
| ķ | U+0137 | K |
| ĸ | U+0138 | K |
| Ĺ | U+0139 | L |
| ĺ | U+013A | L |
| Ļ | U+013B | L |
| ļ | U+013C | L |
| Ľ | U+013D | L |
| ľ | U+013E | L |
| Ŀ | U+013F | L |
| ŀ | U+0140 | L |
| Ł | U+0141 | L |
| ł | U+0142 | L |
| Ń | U+0143 | N |
| ń | U+0144 | N |
| Ņ | U+0145 | N |
| ņ | U+0146 | N |
| Ň | U+0147 | N |
| ň | U+0148 | N |
| ŉ | U+0149 | N |
| Ŋ | U+014A | N |
| ŋ | U+014B | N |
| Ō | U+014C | O |
| ō | U+014D | O |
| Ŏ | U+014E | O |
| ŏ | U+014F | O |
| Ő | U+0150 | O |
| ő | U+0151 | O |
| Œ | U+0152 | OE |
| œ | U+0153 | OE |
| Ŕ | U+0154 | R |
| ŕ | U+0155 | R |
| Ŗ | U+0156 | R |
| ŗ | U+0157 | R |
| Ř | U+0158 | R |
| ř | U+0159 | R |
| Ś | U+015A | S |
| ś | U+015B | S |
| Ŝ | U+015C | S |
| ŝ | U+015D | S |
| Ş | U+015E | S |
| ş | U+015F | S |
| Š | U+0160 | S |
| š | U+0161 | S |
| Ţ | U+0162 | T |
| ţ | U+0163 | T |
| Ť | U+0164 | T |
| ť | U+0165 | T |
| Ŧ | U+0166 | T |
| ŧ | U+0167 | T |
| Ũ | U+0168 | U |
| ũ | U+0169 | U |
| Ū | U+016A | U |
| ū | U+016B | U |
| Ŭ | U+016C | U |
| ŭ | U+016D | U |
| Ů | U+016E | U |
| ů | U+016F | U |
| Ű | U+0170 | U |
| ű | U+0171 | U |
| Ų | U+0172 | U |
| ų | U+0173 | U |
| Ŵ | U+0174 | W |
| ŵ | U+0175 | W |
| Ŷ | U+0176 | Y |
| ŷ | U+0177 | Y |
| Ÿ | U+0178 | Y |
| Ź | U+0179 | Z |
| ź | U+017A | Z |
| Ż | U+017B | Z |
| ż | U+017C | Z |
| Ž | U+017D | Z |
| ž | U+017E | Z |
| ſ | U+017F | S |
| Ș | U+0218 | S |
| ș | U+0219 | S |
| Ț | U+021A | T |
| ț | U+021B | T |
| ẞ | U+1E9E | SS |

## Corrispondenza con GTFS

GTFS e' il formato aperto degli orari del trasporto pubblico (lo pubblicano l'azienda del trasporto pubblico provinciale e molti altri).
Dove un concetto esiste in GTFS, K-Zero lo segue: cosi' un lettore futuro potra' trasformare i dati aperti
nel formato K-Zero senza riscrivere nulla. Il codice GTFS di un elemento va tra i codici esterni
(sistema = chi pubblica i dati, tipo = il nome del campo GTFS).

| K-Zero | GTFS | Note |
|---|---|---|
| manifesto: `aziende[]` | `agency.txt` (`agency_id`, `agency_name`) | |
| manifesto: `fuso_orario` | `agency.txt` `agency_timezone` | Stesso significato: orari locali di quel fuso |
| manifesto: `periodo` | `feed_info.txt` `feed_start_date`, `feed_end_date` | Date AAAA-MM-GG invece di AAAAMMGG |
| luogo di tipo `fermata` | `stops.txt` (`stop_id`, `stop_name`, `stop_lat`, `stop_lon`) | `stop_id` e `stop_code` vanno tra i codici esterni |
| `luoghi[].nomi_alternativi` | nessuno | GTFS ha un solo nome (le traduzioni stanno in `translations.txt`) |
| codice esterno di tipo "linea" (es. B610) | `routes.txt` (`route_id`, `route_short_name`) | La "linea" del committente: contiene piu' corse |
| `corse.json` (una corsa) | `trips.txt` (`trip_id`) | `trip_short_name` (es. "533") tra i codici esterni |
| `corse[].testo_esposto` | `trips.txt` `trip_headsign` | |
| `corse[].passaggi[]` | `stop_times.txt` | Una riga per passaggio, nell'ordine (`stop_sequence`) |
| `passaggi[].orario`, `partenza` | `stop_times.txt` `arrival_time`, `departure_time` | Anche oltre le 24:00, come in GTFS; in K-Zero senza secondi |
| `passaggi[].transito` | `stop_times.txt` `pickup_type` = 1 e `drop_off_type` = 1 | |
| `passaggi[].km` | `stop_times.txt` `shape_dist_traveled` (in parte) | GTFS conta la distanza dall'inizio; K-Zero i km dal passaggio precedente, divisi in pieni e vuoti |
| `calendari.json`: `giorni`, `dal`, `al` | `calendar.txt` (`monday` ... `sunday`, `start_date`, `end_date`) | |
| `date_aggiunte`, `date_tolte` | `calendar_dates.txt` `exception_type` 1 e 2 | |
| `sospensioni` | `calendar_dates.txt` (una riga con `exception_type` 2 per ogni giorno) | GTFS non ha periodi con motivo |
| `calendari[].base`, `aggiunge`, `toglie` | nessuno | In GTFS il calendario va scritto per intero |
| `festivita.json` | nessuno | In GTFS le feste sono date tolte (`calendar_dates.txt`) in ogni calendario |
| `corse[].coincidenze[]` | `transfers.txt` (in parte) | GTFS descrive coincidenze tra le proprie corse; K-Zero con un servizio esterno (es. un treno) |
| servizi, domanda, gruppi, passeggeri | nessuno | GTFS descrive l'offerta pubblica, non il lavoro affidato a un'azienda ne' chi deve viaggiare |
| trasbordi | nessuno | |
| regolamenti, contratti, committenti, gruppi di servizi | nessuno | |
| `aziende.json`, `norme.json`, `persone.json`, `assenze.json`, `fermi.json` | nessuno | GTFS non descrive il personale ne' i mezzi |
| giornata del mezzo (i turni da autista dello stesso mezzo in un giorno) | `trips.txt` `block_id` | In GTFS un `block_id` raggruppa le corse fatte di seguito dallo stesso mezzo; K-Zero lo ricava dai turni |
| `turni.json`, rotazioni, servizi fissi, assegnazioni | nessuno | GTFS non descrive chi guida |
| mezzi, rimesse, tratti con i limiti, tempi di sosta | nessuno | |

## Corrispondenza con Transmodel

Transmodel e' il modello europeo dei dati del trasporto pubblico (norma EN 12896; NeTEx ne e' il formato di
scambio). I nomi di K-Zero sono italiani semplici, ma i concetti dei turni corrispondono:

| K-Zero | Transmodel | Note |
|---|---|---|
| corsa | Vehicle Journey (Service Journey) | |
| giornata del mezzo | Block (Vehicle Block) | Le corse di un mezzo in un giorno, dall'uscita al rientro in rimessa |
| turno del giorno (ruolo autista) | Duty | Il lavoro di un autista in un giorno |
| pezzo di corsa (`da`, `a`) | Run / Stretch | Parte di un Block fatta da un autista |
| luogo del cambio dell'autista | Relief Point (Relief Opportunity) | |
| attivita' senza passeggeri | Dead Run, Layover, Break... | In K-Zero l'elenco e' dell'azienda (aziende.json) |
| rotazione | Rostering (Roster) | Transmodel non fissa la forma del ciclo |
| calendario | Day Type / Operating Day | |

## Glossario

- **Linea** - parola con due sensi. Per il **committente** una linea (es. B634) e' un percorso con un nome,
  che contiene **piu' corse** durante il giorno. Per l'**azienda**, spesso, "la linea delle 7:52" e' una
  sola corsa del committente. In K-Zero: la linea del committente e' un codice esterno di tipo "linea";
  ogni viaggio e' una corsa.
- **Servizio** - il lavoro che un committente affida a un'azienda con un contratto: si paga, si perde,
  si rinuncia, si scambia (di solito una corsa alla volta, a volte un intero lotto). Ha un tipo dichiarato
  dal committente e una domanda.
- **Domanda** - chi deve andare da dove a dove ed entro quando, indipendente dalle corse di oggi: uno o
  piu' gruppi di passeggeri con le fermate e i conteggi.
- **Gruppo di passeggeri** - una parte della domanda con lo stesso tipo (studenti, disabili, utenti di
  linea, adulti autorizzati), la stessa scuola o centro e la stessa finestra oraria. Puo' essere anonimo
  (solo numeri); per le linee il numero puo' essere stimato o non noto.
- **Corsa** - il viaggio vero di un mezzo, con i suoi passaggi. Soddisfa uno o piu' servizi.
- **Corsa promiscua** (fino al K-011 "ibrida") - corsa che soddisfa piu' servizi (anche se uno e' NCC), o che porta
  gruppi di tipi diversi, o studenti verso un centro; non per un servizio NCC, i cui gruppi non cambiano la categoria. Non
  si dichiara: la ricava il programma.
- **NCC** - noleggio con conducente: servizi per gruppi formati su iniziativa del cliente o dell'azienda (gite, turismo,
  eventi, matrimoni), anche se si ripetono (`servizi[].tipo` = `ncc`, sotto).
- **Giro** - nel linguaggio dell'azienda, una delle corse con cui si porta la domanda di un servizio
  (es. "tre giri" per 38 ragazzi con un 16 posti).
- **Passaggio** - un punto della corsa: luogo, orario, persone che salgono e scendono.
- **St. Car. / St. Scar.** - sulle schede del committente: studenti caricati e scaricati a una fermata.
- **Km pieni / km vuoti** - km con passeggeri a bordo / senza (es. dalla rimessa alla prima fermata).
- **Trasbordo** - passaggio di una persona da una corsa (consegna) a un'altra (ricezione) in un punto
  autorizzato dal committente.
- **Coincidenza** - legame con un servizio esterno (es. un treno): aspettare il suo arrivo o garantire
  la sua partenza.
- **Committente** - chi affida i servizi (consorzio, Provincia, Comune, associazione, azienda di linea).
- **Contratto** - l'accordo tra committente e azienda: regolamento, chi fornisce l'assistente, regole sul
  mezzo.
- **Gruppo di servizi / lotto** - servizi affidati insieme con lo stesso contratto.
- **Regolamento** - i parametri di un territorio o di un committente (attese, distanze, minimi,
  assistente...), ognuno con la sua fonte. E' un dato: il programma non ne conosce nessuno.
- **Calendario / periodo** - i giorni in cui vale un servizio o una corsa (es. i periodi "o", "f1"... di
  un committente sono codici esterni dei calendari).
- **Porta a porta** - servizio in cui il passeggero sale e scende a casa sua: il luogo e' privato.
- **Punto di sosta** - dove il mezzo aspetta tra un servizio e l'altro.
- **Rimessa / ricovero esterno** - dove il mezzo passa la notte (anche presso la casa di un autista).
- **Mezzo ipotetico** - mezzo non ancora in flotta, usato negli scenari.
- **Livello del pacchetto** - "solo numeri" oppure "completo" (vedi sopra).
- **Classe di un dato** - comune, personale o particolare (vedi sopra).
- **Pacchetto** - la cartella con il manifesto e i file dei dati.
- **Turno del giorno** - il lavoro di una persona in un giorno: corse (intere o a pezzi) e attivita' in ordine, con
  il mezzo. Chi lo fa lo dicono rotazione, servizio fisso o assegnazione del singolo giorno.
- **Giornata della persona / del mezzo** - i turni di una persona, o quelli che usano un mezzo, in un giorno: due
  viste degli stessi turni.
- **Rotazione** - ciclo di passi (giorni o mesi) che si ripete, con i turni di ogni persona.
- **Servizio fisso** - un turno sempre alla stessa persona nei giorni di un calendario.
- **Assegnazione del singolo giorno** - chi fa un turno, o con quale mezzo, in una data precisa.
- **Turno scoperto** - un turno che in un giorno non ha nessuna persona.
- **Punto di cambio** - il luogo dove un autista lascia la corsa a un altro.
- **Blocco** - corse fissate su un mezzo, per il calcolo futuro.
- **Distrazione** - autorizzazione di un mezzo a un uso diverso da quello della carta di circolazione (es. un bus
  di linea usato per una gita), con l'autorita', il riferimento, le date e le prescrizioni.
