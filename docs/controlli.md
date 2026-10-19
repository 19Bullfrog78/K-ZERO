# Controlli sui pacchetti dati (kzero controlla)

`kzero controlla <cartella>` controlla un pacchetto dati (spiegato in [formato-dati.md](formato-dati.md))
e scrive un resoconto in testo o in JSON (schema `controlla`, file
[schema/controlla.v3.schema.json](schema/controlla.v3.schema.json); le versioni 1, del K-007, e 2, del K-008, non sono piu'
prodotte).

Ogni problema ha:
- un **livello**: **errore** (il dato e' sbagliato e va corretto) oppure **avviso** (il dato e' accettabile,
  ma manca qualcosa che servira' o c'e' qualcosa da guardare);
- un **codice fisso** `K-CTR-nnn`: non cambia mai e non viene mai riusato per un altro controllo;
- un **messaggio in italiano** sul caso preciso;
- la **posizione**: il file e il percorso JSON standard nel file (es. `/corse/3/passaggi/4` = il quinto
  passaggio della quarta corsa: si conta da 0). Per un file che non e' JSON valido, riga e colonna.
  Alcuni problemi riguardano tutto il pacchetto e non hanno posizione.

Come si legge il resoconto:
- prima gli errori, poi gli avvisi; ciascuno in ordine di file (nell'ordine del formato) e di posizione;
  stesso pacchetto = stesso resoconto, byte per byte;
- in fondo un **riepilogo**: aziende, luoghi, servizi, corse, mezzi, persone, turni, errori, avvisi;
- **codice di uscita 0** se non ci sono errori (gli avvisi non bloccano), **1** se c'e' almeno un errore;
- prima si controlla la **forma** dei file (K-CTR-001 ... K-CTR-008, K-CTR-048, K-CTR-053 e K-CTR-088 ... K-CTR-090): un
  file con un errore di forma non viene usato, e finche' c'e' un errore di forma i controlli sul **contenuto** (gli altri codici)
  non si fanno, perche' darebbero risultati falsati. Il resoconto lo dice;
- i messaggi seguono le **classi dei dati**: di base, al posto di un dato personale (es. il nome di una casa)
  c'e' il codice dell'elemento e i dati particolari non compaiono; si vedono con `--includi-dati-personali`
  e `--includi-dati-particolari`. Dal K-009 anche le posizioni, e i problemi sui dati sulla salute non si elencano uno
  per uno (sezione "Dati personali e particolari nei problemi", qui sotto);
- il programma non usa l'orologio: le date sono quelle del pacchetto.

I testi dei messaggi stanno tutti in un unico catalogo (`src/api/controlli_catalogo.cpp`), separato dai
controlli: per tradurli in un'altra lingua basta tradurre il catalogo.

Esempio in testo (pacchetto demo con l'orario `16:08` scritto al posto di `12:07`, codice di uscita 1):

```
Controllo del pacchetto DEMO-2026 "Pacchetto dimostrativo K-Zero" (livello completo)
Errore K-CTR-017 (corse.json, /corse/3/passaggi/2): Nella corsa C-RM-1 (Ritorno mezzogiorno superiori e medie) l'orario 16:08 non sta tra quello del passaggio precedente (12:05) e quello del successivo (12:19): l'orario torna indietro.
Avviso K-CTR-035 (luoghi.json, /luoghi/5): Il luogo F-CASTELROVO-ALTO (Castelrovo, Alto) non ha le coordinate: serviranno al calcolo delle distanze.
Avviso K-CTR-043 (luoghi.json, /luoghi/17/trasbordo): Il punto di trasbordo L-TRASB-CROCE (Croce di Pradello, parcheggio) e' autorizzato solo a voce: manca il documento.
Avviso K-CTR-050 (mezzi.json, /mezzi/6/autorizzazioni_uso/0): L'autorizzazione del mezzo ALFA-05 (AA 105 AA) all'uso NCC (distrazione) scade il 2027-06-30, dentro il periodo del pacchetto: dopo quella data il mezzo non puo' fare i servizi che chiedono quell'uso.
Avviso K-CTR-078 (persone.json, /persone/1; il punto preciso con --includi-dati-personali): Il turno T-PRIMO (Primo) e' scoperto il 2026-11-20: la persona prevista P-B non puo' farlo (dati personali: riservati). Rimedio: un'assegnazione del singolo giorno.
Avviso K-CTR-050 (persone.json, /persone/2; il punto preciso con --includi-dati-personali): Un'abilitazione o un'autorizzazione della persona P-C scade dentro il periodo del pacchetto (dati personali: riservati).
Avviso K-CTR-078 (persone.json, /persone/2; il punto preciso con --includi-dati-personali): Il turno T-SECONDO (Secondo) e' scoperto in 8 giorni tra il 2027-07-07 e il 2027-08-25 (i primi: 2027-07-07, 2027-07-14, 2027-07-21, 2027-07-28, 2027-08-04, 2027-08-11...): la persona prevista P-C non puo' farlo (dati personali: riservati). Rimedio: un'assegnazione del singolo giorno.
Avviso K-CTR-082 (servizi_fissi.json, /servizi_fissi/2): Nel turno T-SCUOLA-1 (Scuola 1) la persona P-E guida il mezzo ALFA-07 (AA 107 AA), non il suo mezzo abituale (abbinamento preferito), dal 2026-10-19 al 2026-10-20 (2 giorni).
Avviso K-CTR-036: Il pacchetto non ha i tempi di sosta (file tempi_sosta.json): serviranno al calcolo.
Riepilogo: 2 aziende, 23 luoghi, 9 servizi, 26 corse, 10 mezzi, 12 persone, 18 turni; 1 errore, 8 avvisi.
```

## Dati personali e particolari nei problemi (K-009)

Un problema puo' far capire un dato anche quando il suo messaggio e' filtrato: il codice K-CTR-047 dice da solo che
un passeggero ha bisogno dell'assistente, la posizione `/persone/2/abilitazioni/2` dice che si parla di una patente,
`assenze.json /assenze/2` dice che la persona e' assente. Per questo la regola vale per il problema intero, non solo
per il testo, ed e' uguale in testo e in JSON.

**Cosa rivela un problema.** Ogni controllo del catalogo dichiara cosa rivela in se' (che ci sia, il suo codice, il
messaggio di base): nessun dato personale ne' particolare, un dato **personale**, oppure un dato **particolare**
(salute); se serve, variante per variante del messaggio. Una prova controlla che tutti lo dichiarino e che la tabella
qui sotto coincida con il programma. Conta poi anche il dato in cui sta la posizione: un problema rivela il piu'
riservato tra cio' che dichiara il controllo e la **classe del dato in cui sta la sua posizione** (la piu' riservata
marcata lungo il percorso JSON, dagli schemi). Es. un codice inesistente (K-CTR-010) scritto tra le competenze richieste
da un servizio, che sono un dato particolare, rivela un dato particolare.

**Problemi sui dati particolari.** Di base non si elencano uno per uno: una riga sola, senza codice ne' posizione, con
quanti errori e quanti avvisi sono e il rimedio:

```
Problemi sui dati particolari (salute), non elencati uno per uno: 1 errore e nessun avviso. Per vederli: --includi-dati-particolari.
```

In JSON e' il campo `problemi_riservati` (`errori`, `avvisi`). Contano lo stesso: sono nel riepilogo, il codice di
uscita non cambia e `kzero giornata` resta bloccata se sono errori (K-ERR-018). Con `--includi-dati-particolari` si
elencano come gli altri.

**Posizioni.** Di base la posizione di un problema non entra mai in un dato personale o particolare: si ferma
all'**elemento comune piu' vicino** (es. la persona in persone.json, non la sua abilitazione; il gruppo o la fermata in
servizi.json, non le sue carrozzine). Un file di soli dati personali (oggi `assenze.json`, marcato tutto intero come
personale nel suo schema) non compare mai: un problema su un'assenza ha la posizione della persona assente in
persone.json (marcatura `x-kzero-titolare` dello schema); se la persona non si trova, la posizione non si mostra. La
regola si ricava dalle classi degli schemi (`x-kzero-classe`), non da un elenco di casi scritto a mano. Quando la
posizione e' ridotta, il testo lo dice ("il punto preciso con --includi-dati-personali") e in JSON c'e' il campo
`posizione_completa_con`; con quel parametro (o `--includi-dati-particolari`, secondo il dato) si vede la posizione
completa. Un campo non previsto dallo schema (es. un nome scritto male) prende la classe dell'elemento che lo contiene:
il nome di un campo non e' un dato. La stessa regola vale per le posizioni mostrate da `kzero giornata`; gli errori del
programma (K-ERR), che non hanno percorsi JSON, non nominano mai un file di soli dati personali.

Controlli che rivelano un dato personale o particolare (gli altri non ne rivelano nessuno; la loro posizione segue
comunque la regola qui sopra):

| Codice | Rivela | Quando | Cosa |
|---|---|---|---|
| K-CTR-019 | personale | alcuni casi | il nome, la casa o una nota scritta a mano di una persona |
| K-CTR-031 | particolare | alcuni casi | carrozzine e ausili piegati che non corrispondono ai passeggeri |
| K-CTR-047 | particolare | tutti i casi | l'assistente di cui ha bisogno un passeggero |
| K-CTR-050 | personale | alcuni casi | la scadenza di un'abilitazione o di un'autorizzazione di una persona |
| K-CTR-067 | personale | alcuni casi | assenza, disponibilita', abilitazione o autorizzazione scaduta di una persona |
| K-CTR-068 | personale | tutti i casi | l'abilitazione o l'autorizzazione che una persona non ha |
| K-CTR-071 | particolare | tutti i casi | la competenza richiesta da un turno o da un servizio (puo' far capire chi viaggia) |
| K-CTR-076 | personale | tutti i casi | i ruoli di una persona |
| K-CTR-078 | personale | alcuni casi | assenza, disponibilita', abilitazione o autorizzazione scaduta di una persona |
| K-CTR-081 | personale | tutti i casi | le preferenze di una persona |

Non sono in questo catalogo (arriveranno con la verifica delle regole e con il mandato "orari di lavoro"):
posti a bordo, attese massime sulle corse, limiti di accesso confrontati con i mezzi, tempi per andare da un
luogo all'altro tra due turni, km calcolati, limiti di legge sugli orari di lavoro e sulle pause.

## Controlli sui turni, giorno per giorno

I controlli sui turni (K-CTR-054 ... K-CTR-087) guardano ogni giorno del periodo del pacchetto: chi fa ogni
turno (assegnazione del singolo giorno, poi servizio fisso, poi rotazione: vince il piu' preciso), con quale
mezzo, con quali corse e attivita'. Seguono un principio unico:
- un dato che **contraddice** un altro dato, o che e' **sempre sbagliato** (un autista senza la patente per quel
  mezzo, un mezzo senza l'uso richiesto, una persona in due turni insieme) e' un **errore**;
- un **fatto con le date** (assenza, fuori dalla disponibilita', scadenza, fermo, distrazione o prestito non
  validi) che colpisce un turno **ricorrente** (rotazione, servizio fisso, mezzo scritto nel turno) e' un
  **avviso**: quel giorno il turno e' scoperto o senza mezzo, e il rimedio e' un'assegnazione del singolo giorno.
  Lo stesso fatto su un'**assegnazione del singolo giorno** e' un **errore** (K-CTR-067): l'utente ha scelto
  quel giorno preciso e i due dati si contraddicono;
- un turno che va contro un'**impostazione** o una **preferenza** e' un avviso; contro l'impostazione "fisso" o
  "mai" dell'azienda, su un turno ricorrente, e' un errore (la scelta dell'azienda vince); su un'assegnazione del
  singolo giorno torna un avviso (la scelta dell'utente vince);
- un piano **incompleto** (turno scoperto, corsa senza turno, mezzo da assegnare) e' un avviso.

**Problemi uguali in molti giorni.** Lo stesso problema nello stesso punto dei dati (stesso codice, stessa
posizione, stesso messaggio a parte i giorni) si scrive una volta sola, con i giorni raggruppati in periodi
di giorni consecutivi: "il 2026-10-22", "dal 2026-10-19 al 2026-10-23 (5 giorni)", "in 3 giorni (2026-10-12,
dal 2026-10-19 al 2026-10-20)"; se i periodi sono piu' di sei: "in N giorni tra il X e il Y (i primi: ...)".
In JSON il problema ha anche il campo `giorni`, con tutti i periodi (`dal`, `al`). La posizione e' scelta perche' resti la stessa nei
giorni diversi: la riga della rotazione, il servizio fisso, l'assegnazione, il fatto (assenza, fermo), oppure
il turno. Un turno scoperto tutti i giorni feriali di un anno e' quindi un solo avviso, non 261.

## Elenco completo

Questo elenco deve coincidere con `kzero descrivi`: una prova automatica
(`documentazione.errori_e_schemi`) controlla le prime tre colonne.
Un nuovo controllo prende il codice successivo all'ultimo.

| Codice | Livello | Descrizione | Quando succede | Cosa fare |
|---|---|---|---|---|
| K-CTR-001 | errore | Manca il manifesto | La cartella non contiene `pacchetto.json`. | Aggiungere il manifesto (vedi formato-dati.md). |
| K-CTR-002 | errore | File illeggibile | Un file del pacchetto c'e' ma non si riesce a leggere (permessi, disco), oppure e' un collegamento (link) che non porta a nessun file. | Controllare i permessi del file e riprovare; rifare il collegamento o mettere al suo posto il file. |
| K-CTR-003 | errore | JSON non valido | Il file non e' JSON valido: virgola o parentesi mancante, virgolette non chiuse... Riga e colonna indicano dove il programma se ne accorge: l'errore e' li' o poco prima. | Correggere la scrittura del file alla riga indicata o subito prima. |
| K-CTR-004 | errore | Campo ripetuto | Lo stesso campo compare due volte nello stesso oggetto (il JSON lo permetterebbe, ma uno dei due valori andrebbe perso in silenzio). | Tenere un solo valore. |
| K-CTR-005 | errore | Schema del file sbagliato o mancante | Manca il campo `schema`, oppure dichiara uno schema diverso da quello del file (es. `luoghi.json` con lo schema `fermate`). | Scrivere lo schema giusto per quel file (tabella in formato-dati.md). |
| K-CTR-006 | errore | Versione dello schema non supportata | La `schema_versione` del file non e' tra quelle che questo programma sa leggere, oppure manca. Il file non viene letto, nemmeno in parte. | Usare un programma aggiornato, oppure scrivere il file nella versione indicata nel messaggio. |
| K-CTR-007 | errore | File non conforme allo schema | Il file non rispetta il suo schema: campo obbligatorio mancante, campo non previsto, valore non ammesso, forma sbagliata (es. orario `12.05`), numero fuori dai limiti. Il messaggio dice cosa e dove. Il valore sbagliato di un campo personale o particolare compare solo con il parametro che lo permette. | Correggere il campo indicato dal percorso JSON. |
| K-CTR-008 | avviso | File non riconosciuto | Nella cartella c'e' un file o una cartella che non fa parte del formato (spesso un nome scritto male, es. `fermate.json`). Non viene letto. | Rinominarlo con il nome giusto o toglierlo. |
| K-CTR-009 | errore | Codice doppio | Due elementi dello stesso tipo hanno lo stesso codice (i gruppi di passeggeri e i passeggeri in tutto il pacchetto). | Dare a ognuno un codice diverso. |
| K-CTR-010 | errore | Riferimento a un codice inesistente | Un dato nomina un codice che non esiste tra gli elementi di quel tipo (es. una corsa che passa da `F-PRADELO`). Per le abilitazioni e gli usi dei mezzi il codice va cercato nelle norme del paese giuste (quelle dell'azienda del mezzo o le stesse norme del requisito); per i tipi di attivita', di assenza, di fermo e le competenze nell'elenco dell'azienda giusta (ogni azienda ha il suo). | Correggere il codice o aggiungere l'elemento mancante (anche nell'elenco dell'azienda o delle norme). |
| K-CTR-011 | errore | Luogo del tipo sbagliato | Una scuola o un centro sta in un luogo che non e' di tipo `scuola` o `centro`; una rimessa in un luogo che non e' `rimessa` o `ricovero_esterno`. | Correggere il luogo o il suo tipo. |
| K-CTR-012 | errore | Fine prima dell'inizio | Un periodo (manifesto, calendario, sospensione, orario di scuola, regolamento, contratto, distrazione, prestito, assenza, fermo, ore, disponibilita', costo, preferenza, rotazione, blocco...) finisce prima di cominciare, oppure un orario (lezioni, finestra di un gruppo, fascia, attivita' di un turno) finisce prima di cominciare. Le date di un dato personale (es. un'assenza) compaiono solo con `--includi-dati-personali`. | Scambiare o correggere le due date o i due orari. |
| K-CTR-013 | errore | Data fuori dal periodo del calendario | Una data aggiunta o tolta, o una sospensione, cade fuori dal periodo del suo calendario. | Correggere la data o allargare il periodo. |
| K-CTR-014 | errore | Data contraddittoria nel calendario | Nello stesso calendario una data e' sia aggiunta sia tolta, oppure una data aggiunta cade in una sospensione (le date tolte e le sospensioni prevalgono). | Decidere se in quel giorno il calendario vale, e scriverlo una volta sola. |
| K-CTR-015 | errore | Calendario senza nessun giorno | Il calendario non vale in nessun giorno del periodo del pacchetto. | Controllare giorni della settimana, periodo, date tolte, sospensioni e calendario di base. |
| K-CTR-016 | errore | Calendario basato su se stesso | Il calendario si basa su se stesso, direttamente o attraverso altri calendari (A ha base B, B ha base A). | Togliere la base che chiude il giro. |
| K-CTR-017 | errore | Orario che torna indietro in una corsa | Dentro una corsa un orario viene prima di quello del passaggio precedente (caso reale: `16:08` scritto tra `12:05` e `12:19`). Quando il passaggio dopo torna in ordine, il messaggio indica proprio l'orario fuori posto. Anche una partenza prima dell'arrivo nello stesso passaggio. | Correggere l'orario indicato (dopo mezzanotte si scrive 24:05, non 00:05). |
| K-CTR-018 | errore | Nome o codice esterno di fermata che vale per due fermate | Dopo la normalizzazione (maiuscole, accenti, punteggiatura, spazi) lo stesso nome, nome alternativo o codice esterno appartiene a due fermate: la ricerca non saprebbe quale scegliere. | Distinguere i nomi (es. aggiungere la via) o correggere il codice esterno. |
| K-CTR-019 | errore | Dati personali in un pacchetto "solo numeri" | Il pacchetto e' di livello `solo_numeri` ma contiene luoghi privati (case, punti porta a porta), passeggeri uno per uno, oppure il nome, la casa o una nota di una persona (autista o assistente), oppure una nota scritta a mano di una rotazione, di un servizio fisso o di un'assegnazione del singolo giorno (puo' dire il motivo di un cambio). | Togliere quei dati, oppure dichiarare il livello `completo`. |
| K-CTR-020 | errore | Persone a bordo sotto zero | In una corsa scendono piu' persone di un gruppo di quante ne sono salite (contando anche i trasbordi). | Correggere i conteggi di salita e discesa della corsa. |
| K-CTR-021 | errore | Attesa concordata fuori dai limiti del regolamento | L'attesa concordata con una scuola (prima o dopo le lezioni) supera il massimo del regolamento che vale per il contratto dei servizi che la usano. Lo stesso dato puo' andare bene con un regolamento e non con un altro. | Ridurre l'attesa concordata o controllare il regolamento del contratto. |
| K-CTR-022 | errore | Mezzo massimo piu' piccolo del mezzo minimo | Il mezzo massimo ammesso ha meno posti del mezzo minimo, nel contratto o nel servizio (che cambia una sola delle due regole del contratto). | Correggere i posti minimi o massimi. |
| K-CTR-023 | errore | Trasbordo in un luogo non autorizzato | Un trasbordo avviene in un luogo che non e' un punto di trasbordo autorizzato (blocco `trasbordo` del luogo), o in una casa. | Spostare il trasbordo in un punto autorizzato o registrare l'autorizzazione. |
| K-CTR-024 | errore | Trasbordo senza la sua meta' | Un trasbordo ha la consegna ma nessuna corsa lo riceve, o viceversa. | Scrivere l'altra meta' con lo stesso codice, o correggere il codice. |
| K-CTR-025 | errore | Corsa che riceve un trasbordo arrivando dopo la corsa che consegna | La corsa che riceve arriva dopo il limite: deve essere sul posto prima della corsa che consegna, con il margine del manifesto (`margine_trasbordo_minuti`). | Anticipare la corsa che riceve o posticipare quella che consegna. |
| K-CTR-026 | errore | Meta' del trasbordo che non corrispondono | Le due meta' di un trasbordo non corrispondono: piu' consegne o ricezioni con lo stesso codice, consegna e ricezione sulla stessa corsa, luoghi diversi, gruppo, persone o passeggero diversi. | Allineare le due meta'. |
| K-CTR-027 | errore | Corsa che riceve un trasbordo e riparte prima della consegna | La corsa che riceve riparte prima che arrivi la corsa che consegna. | Correggere la partenza della corsa che riceve. |
| K-CTR-028 | errore | Coincidenza da aspettare: partenza prima dell'arrivo | Coincidenza "aspettare l'arrivo": la corsa parte prima dell'arrivo del servizio esterno. | Posticipare la partenza o correggere l'orario della coincidenza. |
| K-CTR-029 | errore | Coincidenza da garantire: arrivo dopo la partenza | Coincidenza "garantire la partenza": la corsa arriva dopo la partenza del servizio esterno. | Anticipare la corsa o correggere l'orario della coincidenza. |
| K-CTR-030 | errore | Coincidenza in un luogo dove la corsa non passa | La coincidenza e' in un luogo dove la corsa non passa. | Correggere il luogo della coincidenza. |
| K-CTR-031 | errore | Conteggi del gruppo diversi dai passeggeri | I conteggi di una fermata del gruppo (salgono, scendono, carrozzine, ausili piegati) non corrispondono ai passeggeri uno per uno che salgono o scendono li' (i sospesi non contano), oppure un passeggero sale o scende in un luogo che non e' tra le fermate del gruppo. Le carrozzine sono le persone che viaggiano sedute sulla carrozzina; gli ausili piegati sono quelli da caricare, carrozzine piegate comprese. Senza `--includi-dati-particolari` il messaggio su carrozzine e ausili non li nomina. | Allineare i conteggi e i passeggeri. |
| K-CTR-032 | errore | Servizio di un altro contratto in un gruppo di servizi | Un gruppo di servizi contiene un servizio di un altro contratto. | Spostare il servizio nel gruppo giusto o correggere il contratto. |
| K-CTR-033 | errore | Servizio in due gruppi di servizi | Lo stesso servizio sta in due gruppi di servizi. | Lasciarlo in un solo gruppo. |
| K-CTR-034 | errore | Corsa che trasporta un gruppo di un servizio che non soddisfa | Una corsa fa salire o scendere un gruppo di un servizio che la corsa non soddisfa. | Aggiungere il servizio alla corsa o correggere il gruppo. |
| K-CTR-035 | avviso | Luogo senza coordinate | Un luogo senza coordinate: servono al calcolo delle distanze. | Aggiungere le coordinate quando si conoscono. |
| K-CTR-036 | avviso | Tempi di sosta non definiti | Mancano i tempi di sosta (file `tempi_sosta.json`), oppure sono incompleti. | Aggiungerli quando sono confermati. |
| K-CTR-037 | avviso | Durata massima a bordo non definita | Per un gruppo di un servizio con orari liberi nessuno fissa la durata massima a bordo: ne' la scuola, ne' il servizio, ne' il contratto, ne' il regolamento per il grado della scuola. | Aggiungere la durata massima dove e' stabilita. |
| K-CTR-038 | avviso | Fermata mai usata | Una fermata pubblica non e' usata da nessuna corsa, coincidenza o domanda. | Toglierla se non serve piu'. |
| K-CTR-039 | avviso | Servizio senza corse | Nessuna corsa soddisfa il servizio. | Aggiungere le corse, o toglierlo se il servizio e' stato perso. |
| K-CTR-040 | avviso | Corsa che non soddisfa nessun servizio | La corsa non soddisfa nessun servizio. | Indicare i servizi della corsa. |
| K-CTR-041 | avviso | Gruppo con zero passeggeri | Un gruppo con conteggi esatti non ha nessuna persona ne' passeggeri. | Completare la domanda o togliere il gruppo. |
| K-CTR-042 | avviso | Domanda diversa dalle persone delle corse | Le persone che salgono o scendono a una fermata secondo la domanda sono diverse da quelle caricate dalle corse (es. domanda 8, corse 7). Il confronto si fa calendario per calendario: corse con lo stesso calendario si sommano. | Controllare se la domanda o le corse sono da aggiornare. |
| K-CTR-043 | avviso | Punto di trasbordo autorizzato a voce, senza documento | Il punto di trasbordo e' autorizzato solo a voce. | Chiedere al committente il documento dell'autorizzazione. |
| K-CTR-044 | avviso | Mezzo senza costo fisso giornaliero o senza costo al km | Al mezzo manca il costo fisso giornaliero, il costo al km o entrambi: senza, il confronto dei costi sarebbe falso. | Aggiungere i costi. |
| K-CTR-045 | avviso | Azienda senza mezzi | Un'azienda del manifesto non ha mezzi nel pacchetto. | Aggiungere i mezzi o togliere l'azienda. |
| K-CTR-046 | avviso | Contratto senza regolamento valido per una parte del periodo | Il regolamento del contratto non vale per una parte del periodo del contratto (dentro il periodo del pacchetto). | Indicare il regolamento valido o correggere le date. |
| K-CTR-047 | errore | Passeggero con un'esigenza che il suo gruppo non riporta | Un passeggero che viaggia (non sospeso) ha bisogno dell'assistente a bordo, ma il suo gruppo non richiede l'assistente (`assistente_richiesto`). Senza `--includi-dati-particolari` il messaggio non nomina l'assistente. | Indicare `assistente_richiesto: true` nel gruppo, o correggere le esigenze del passeggero. |
| K-CTR-048 | errore | File non salvato in UTF-8 | Il file non e' salvato in UTF-8, la codifica dei file JSON: di solito e' stato salvato come "ANSI" (lettere accentate scritte in un'altra codifica) o come "Unicode" (UTF-16) dal Blocco note o da un programma vecchio. Riga e colonna indicano il primo carattere sbagliato; il messaggio non riporta il testo del file. Il file non viene letto. | Aprire il file e salvarlo di nuovo scegliendo la codifica UTF-8. |
| K-CTR-049 | errore | Calendario combinato con se stesso | Un calendario aggiunge o toglie i giorni di se stesso, direttamente o attraverso altri calendari (es. A aggiunge B, B ha base C, C toglie A): non si puo' sapere in quali giorni vale. Un giro fatto di sole basi e' K-CTR-016. | Togliere il collegamento (aggiunge, toglie o base) che chiude il giro. |
| K-CTR-050 | avviso | Scadenza dentro il periodo del pacchetto | Un'autorizzazione con una scadenza finisce prima dell'ultimo giorno del periodo del pacchetto: l'autorizzazione di un mezzo a un uso in piu' (distrazione); un'abilitazione o un'autorizzazione del committente di una persona. Dopo quella data cio' che la richiede non si puo' piu' fare. | Rinnovarla in tempo e scrivere la nuova scadenza. |
| K-CTR-051 | avviso | Mezzo senza l'uso della carta di circolazione | Le norme del paese dell'azienda dicono quali usi dei mezzi servono (per tipo di servizio o per i requisiti di guida), ma il mezzo non ha l'`uso`: quei controlli non si possono fare. | Scrivere l'uso della carta di circolazione del mezzo. |
| K-CTR-052 | avviso | Azienda senza le norme del paese | I dati dell'azienda usano cose che si controllano con le norme del paese (uso dei mezzi, distrazioni, abilitazioni delle persone, autisti dei turni), ma l'azienda non indica le sue norme in `aziende.json`. | Indicare le norme dell'azienda (campo `norme` in aziende.json). |
| K-CTR-053 | errore | File troppo grande | Il file e' piu' grande di 256 MB: e' certamente un errore (un file sbagliato o prodotto da un altro programma), e leggerlo potrebbe esaurire la memoria del PC. Il file non viene letto, o la lettura si ferma appena passa il massimo (anche quando il sistema non dice la dimensione o il file cresce mentre lo si legge). | Controllare il file: un pacchetto vero sta in pochi MB per file. |
| K-CTR-054 | errore | Pezzo di corsa che non si trova nella corsa | Un turno fa solo un pezzo di una corsa (cambio dell'autista a meta' corsa), ma il luogo in cui il pezzo comincia o finisce non e' tra i passaggi della corsa; oppure la corsa ci passa piu' volte e non c'e' l'orario per dire quale passaggio; oppure il pezzo finisce prima di cominciare. | Correggere il luogo, o aggiungere l'orario del passaggio (`da.orario`, `a.orario`). |
| K-CTR-055 | errore | Corse e attivita' di un turno fuori ordine o sovrapposte | Nel turno un elemento (corsa o attivita') comincia prima del precedente: vanno scritti in ordine di orario. Oppure, in un giorno, un elemento comincia prima che finisca il precedente (es. pulizia 13:00-14:30 e corsa alle 14:00). | Riordinare gli elementi o correggere gli orari. |
| K-CTR-056 | errore | Mezzo del turno che non va con il suo ruolo | Un turno da autista con delle corse ha il mezzo "nessuno"; un turno da assistente o da secondo autista ha un mezzo (il suo mezzo e' quello del turno dell'autista); un turno con il mezzo abituale e' fatto da una persona che non ha un mezzo abituale. | Correggere il mezzo del turno o il mezzo abituale della persona. |
| K-CTR-057 | errore | Rotazione scritta in modo incoerente | Nella rotazione una riga ha un numero di passi diverso dalla lunghezza del ciclo; una persona compare in piu' righe; una rotazione con i passi di un mese non parte il primo giorno di un mese. | Correggere la riga, la lunghezza o la data di partenza. |
| K-CTR-058 | errore | Assegnazione in un giorno in cui il turno non c'e' | Un'assegnazione del singolo giorno e' fuori dal periodo del pacchetto, oppure in un giorno in cui il turno non c'e' (il suo calendario non vale, o quel giorno non ha nessuna corsa e nessuna attivita'). | Correggere la data o il turno dell'assegnazione. |
| K-CTR-059 | errore | Turno in un ricovero esterno con l'impostazione "mai" | Il turno comincia o finisce in un ricovero esterno, ma l'azienda ha scritto in `aziende.json` che i mezzi tornano sempre in rimessa (ricovero vicino a casa: "mai"). La scelta dell'azienda vince. | Far cominciare e finire il turno in rimessa, o cambiare l'impostazione dell'azienda. |
| K-CTR-060 | errore | Periodo del pacchetto troppo lungo per i turni | Il pacchetto ha dei turni e il suo periodo dura piu' di tre anni (1096 giorni): e' certamente un errore di scrittura delle date, e il controllo giorno per giorno sarebbe lentissimo. I controlli sui turni giorno per giorno non si fanno. | Correggere il periodo del manifesto (di solito un anno scolastico o un anno). |
| K-CTR-061 | errore | Corsa divisa in pezzi con un buco | Una corsa e' divisa in pezzi tra piu' turni (cambio dell'autista), ma in un giorno nessun turno la fa da un luogo a un altro: per quel tratto la corsa non ha autista. | Far cominciare il pezzo seguente dove finisce il precedente. |
| K-CTR-062 | errore | Stessa corsa o pezzo di corsa in due turni nello stesso giorno | La stessa corsa, o una sua parte, e' in due turni da autista nello stesso giorno: due autisti per lo stesso tratto. (Il secondo autista ha un turno da `secondo_autista`, che non conta qui.) | Togliere la corsa da uno dei due turni, o correggere i calendari. |
| K-CTR-063 | errore | Persona in due turni che si sovrappongono | La stessa persona fa due turni i cui orari si sovrappongono, nello stesso giorno o a cavallo della mezzanotte (un turno che finisce dopo le 24:00 e uno del giorno dopo che comincia prima). Si guardano le date vere, giorno per giorno: vale anche tra la fine e l'inizio di un ciclo di rotazione e tra due rotazioni. La posizione e' il punto in cui e' scritta l'assegnazione (riga della rotazione, servizio fisso, assegnazione del giorno). | Spostare uno dei due turni a un'altra persona, o correggere gli orari. |
| K-CTR-064 | errore | Mezzo in due turni che si sovrappongono | Lo stesso mezzo e' in due turni da autista i cui orari (da quando il turno impegna il mezzo a quando lo lascia) si sovrappongono, anche a cavallo della mezzanotte. Un cambio di autista sullo stesso mezzo, con l'orario di fine dell'uno uguale a quello di inizio dell'altro, non e' una sovrapposizione. | Dare a uno dei due turni un altro mezzo, o correggere gli orari. |
| K-CTR-065 | errore | Due persone nello stesso turno nello stesso giorno | Due righe di rotazione (della stessa rotazione o di due rotazioni valide insieme) danno lo stesso turno a due persone diverse nello stesso giorno. | Correggere i passi della rotazione. |
| K-CTR-066 | errore | Due servizi fissi o due assegnazioni del giorno sullo stesso turno | Lo stesso turno ha due servizi fissi validi nello stesso giorno, oppure due assegnazioni del singolo giorno nella stessa data. | Tenerne uno solo (un'assegnazione puo' dare persona e mezzo insieme). |
| K-CTR-067 | errore | Assegnazione del singolo giorno contro un fatto | Un'assegnazione del singolo giorno da' il turno a una persona che quel giorno e' assente, non e' disponibile in quegli orari, ha il prestito all'azienda non valido, ha un'abilitazione o un'autorizzazione del committente scaduta; oppure da' un mezzo in fermo, senza l'autorizzazione all'uso (scaduta) o con il prestito non valido. L'utente ha scelto quel giorno preciso: i due dati si contraddicono. I dettagli personali (tipo di assenza, orari, abilitazione) si vedono con `--includi-dati-personali`. | Correggere l'assegnazione o il fatto (assenza, fermo, scadenza). |
| K-CTR-068 | errore | Autista senza l'abilitazione o l'autorizzazione richiesta | Secondo le norme del paese dell'azienda (`norme.json`, requisiti di guida) per guidare quel mezzo serve un'abilitazione che la persona non ha mai avuto (non scaduta: proprio assente); oppure un servizio del turno chiede l'autorizzazione del committente (regolamento) e la persona non ce l'ha. Vale anche per il secondo autista. | Assegnare il turno a una persona abilitata, o aggiungere l'abilitazione se c'e'. |
| K-CTR-069 | errore | Mezzo senza l'uso richiesto dal servizio | Il servizio di una corsa chiede certi usi dei mezzi (regolamento del contratto, o norme dell'azienda del contratto, per tipo di servizio), ma l'uso della carta di circolazione del mezzo non e' tra quelli e il mezzo non ha nessuna autorizzazione a un uso ammesso (distrazione). Una distrazione che c'e' ma e' scaduta quel giorno e' un fatto con le date: K-CTR-079 o K-CTR-067. | Usare un mezzo con l'uso giusto, o scrivere l'autorizzazione (distrazione) del mezzo. |
| K-CTR-070 | errore | Turno ricorrente contro l'abbinamento autista-mezzo fisso | L'azienda ha l'abbinamento autista-mezzo "fisso", ma un turno ricorrente (rotazione o servizio fisso) fa guidare alla persona un mezzo diverso dal suo mezzo abituale, o la persona non ha un mezzo abituale. Con un'assegnazione del singolo giorno e' un avviso (K-CTR-082): la scelta dell'utente vince. | Correggere il mezzo del turno o il mezzo abituale, o cambiare l'impostazione dell'azienda. |
| K-CTR-071 | errore | Persona senza una competenza richiesta | Il turno, o (per l'autista) il servizio di una sua corsa, richiede una competenza (es. uso della pedana) che la persona non ha. La competenza e' un dato particolare: si vede con `--includi-dati-particolari`. | Assegnare il turno a chi ha la competenza, o aggiungerla alla persona. |
| K-CTR-072 | errore | Mezzo usato contro il suo vincolo di destinazione | Il mezzo ha un vincolo di destinazione valido quel giorno (es. comprato con un contributo per un certo tipo di servizio o contratto), ma fa una corsa di un servizio che il vincolo non ammette. | Usare un altro mezzo per quella corsa. |
| K-CTR-073 | errore | Persona o mezzo di un'altra azienda senza prestito dichiarato | Il turno e' di un'azienda, ma la persona o il mezzo sono di un'altra azienda e non hanno nessun prestito verso quella azienda (`prestiti`). Un prestito che c'e' ma non vale quel giorno e' un fatto con le date (K-CTR-078, K-CTR-079 o K-CTR-067). | Dichiarare il prestito con l'accordo, o usare una persona o un mezzo dell'azienda. |
| K-CTR-074 | errore | Corsa bloccata su un mezzo e fatta con un altro | Un blocco (`blocchi.json`) fissa la corsa su un mezzo, ma in un giorno in cui il blocco vale il turno che la fa usa un altro mezzo. | Usare il mezzo del blocco, o togliere il blocco. |
| K-CTR-075 | errore | Due blocchi incompatibili sulla stessa corsa | Due blocchi fissano la stessa corsa su due mezzi diversi in periodi che si sovrappongono. | Togliere uno dei due blocchi o separarne le date. |
| K-CTR-076 | errore | Persona con un ruolo diverso da quello del turno | Un turno da autista (o da secondo autista) e' fatto da una persona che non e' autista, o un turno da assistente da una persona che non e' assistente. Il ruolo e' un dato personale: il messaggio lo nasconde senza `--includi-dati-personali`. | Assegnare il turno a una persona con il ruolo giusto, o aggiungere il ruolo. |
| K-CTR-077 | avviso | Turno scoperto | In uno o piu' giorni nessuno dei tre modi (assegnazione del singolo giorno, servizio fisso, rotazione) da' una persona al turno. Il programma non inventa assegnazioni. | Aggiungere un'assegnazione, un servizio fisso o una riga di rotazione. |
| K-CTR-078 | avviso | Turno ricorrente scoperto per un fatto con le date | La persona che la rotazione o il servizio fisso danno al turno quel giorno non puo' farlo per un fatto con le date: assenza, fuori dalla disponibilita', prestito non valido, abilitazione o autorizzazione del committente scaduta. Il turno quel giorno e' scoperto. La posizione e' il fatto (l'assenza, la scadenza). | Un'assegnazione del singolo giorno con un'altra persona (sostituzione). |
| K-CTR-079 | avviso | Turno ricorrente senza mezzo per un fatto con le date | Il mezzo scritto nel turno (o il mezzo abituale della persona) quel giorno non puo' fare il turno: fermo (revisione, manutenzione), autorizzazione all'uso (distrazione) non valida quel giorno, prestito non valido. Il turno quel giorno e' senza mezzo. | Un'assegnazione del singolo giorno con un altro mezzo. |
| K-CTR-080 | avviso | Corsa senza turno | In uno o piu' giorni in cui la corsa c'e', nessun turno da autista la fa. | Mettere la corsa in un turno, o correggere i calendari. |
| K-CTR-081 | avviso | Preferenza di una persona non rispettata | Un turno va contro una preferenza della persona (giorni liberi, fascia oraria, turni da evitare, pausa massima). Una preferenza non e' mai un obbligo; se l'assegnazione e' del singolo giorno, la scelta dell'utente vince. | Guardare se si puo' dare il turno a un altro, o lasciarlo cosi'. |
| K-CTR-082 | avviso | Abbinamento autista-mezzo non rispettato | Con l'abbinamento "preferito" la persona guida un mezzo diverso dal suo mezzo abituale; oppure, con l'abbinamento "fisso", un'assegnazione del singolo giorno mette la persona su un altro mezzo (la scelta dell'utente vince). | Guardare se si puo' usare il mezzo abituale. |
| K-CTR-083 | avviso | Turno con il mezzo da assegnare | Il turno ha il mezzo "da assegnare" e in quel giorno nessuna assegnazione del singolo giorno gli da' un mezzo. | Scegliere il mezzo con un'assegnazione o scriverlo nel turno. |
| K-CTR-084 | avviso | Persona senza turni | Una persona (non ipotetica) non fa nessun turno in tutto il periodo del pacchetto. | Controllare se manca un'assegnazione, o se la persona non lavora piu'. |
| K-CTR-085 | avviso | Mezzo mai usato che non e' di scorta | Un mezzo (non ipotetico) non e' in nessun turno nel periodo e non e' segnato come mezzo di scorta (`scorta: true`). Si controlla solo se il pacchetto ha dei turni. | Segnarlo come mezzo di scorta, o toglierlo. |
| K-CTR-086 | avviso | Corsa con l'assistente dell'azienda senza persona assegnata | Una corsa ha l'assistente a bordo fornito dall'azienda (dal turno o dal contratto), ma nessun turno da assistente con una persona copre quella corsa quel giorno. | Aggiungere il turno da assistente con la persona. |
| K-CTR-087 | avviso | Assistente fornito da chi il contratto non dice | Il turno dice che l'assistente di una corsa lo fornisce l'azienda (o il committente), ma il contratto dice l'altro; oppure c'e' un turno da assistente dell'azienda su una corsa in cui il contratto dice che l'assistente e' del committente. La scelta scritta nel turno vince, ma va guardata. | Correggere il turno o il contratto. |
| K-CTR-088 | errore | Non e' un file normale | Al posto di un file del pacchetto c'e' qualcosa che non e' un file normale: un dispositivo del sistema (es. `/dev/zero`, che non finisce mai), una "pipe", o un collegamento che porta a una di queste cose. Leggerlo potrebbe bloccare il programma o esaurire la memoria: non viene letto. | Mettere al suo posto il file di dati. |
| K-CTR-089 | errore | Collegamento a un file fuori dalla cartella del pacchetto | Il file e' un collegamento (link simbolico) a un file che sta fuori dalla cartella del pacchetto: il pacchetto deve contenere tutti i suoi dati, e un collegamento preparato apposta potrebbe far leggere al programma un file qualsiasi del PC. Non viene letto. Un collegamento a un file dentro la cartella (anche in una sottocartella) va bene, e la cartella del pacchetto puo' essere a sua volta un collegamento (es. una cartella sincronizzata). | Copiare il file dentro la cartella del pacchetto. |
| K-CTR-090 | errore | Cartella al posto di un file del pacchetto | Nella cartella del pacchetto c'e' una cartella con il nome di un file del formato (es. `persone.json`). Il file conta come presente ma non letto: i controlli sul contenuto non partono, cosi' non ci sono errori a catena sui codici che quel file avrebbe dato. | Togliere o rinominare la cartella e mettere al suo posto il file. |
