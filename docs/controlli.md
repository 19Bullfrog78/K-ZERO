# Controlli sui pacchetti dati (kzero controlla)

`kzero controlla <cartella>` controlla un pacchetto dati (spiegato in [formato-dati.md](formato-dati.md))
e scrive un resoconto in testo o in JSON (schema `controlla`, file
[schema/controlla.v1.schema.json](schema/controlla.v1.schema.json)).

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
- in fondo un **riepilogo**: aziende, luoghi, servizi, corse, errori, avvisi;
- **codice di uscita 0** se non ci sono errori (gli avvisi non bloccano), **1** se c'e' almeno un errore;
- prima si controlla la **forma** dei file (K-CTR-001 ... K-CTR-008 e K-CTR-048): un file con un errore di
  forma non viene usato, e finche' c'e' un errore di forma i controlli sul **contenuto** (K-CTR-009 ...
  K-CTR-047) non si fanno, perche' darebbero risultati falsati. Il resoconto lo dice;
- i messaggi seguono le **classi dei dati**: di base, al posto di un dato personale (es. il nome di una casa)
  c'e' il codice dell'elemento e i dati particolari non compaiono; si vedono con `--includi-dati-personali`
  e `--includi-dati-particolari`;
- il programma non usa l'orologio: le date sono quelle del pacchetto.

I testi dei messaggi stanno tutti in un unico catalogo (`src/api/controlli_catalogo.cpp`), separato dai
controlli: per tradurli in un'altra lingua basta tradurre il catalogo.

Esempio in testo (pacchetto demo con l'orario `16:08` scritto al posto di `12:07`, codice di uscita 1):

```
Controllo del pacchetto DEMO-2026 "Pacchetto dimostrativo K-Zero" (livello completo)
Errore K-CTR-017 (corse.json, /corse/3/passaggi/2): Nella corsa C-RM-1 (Ritorno mezzogiorno superiori e medie) l'orario 16:08 non sta tra quello del passaggio precedente (12:05) e quello del successivo (12:19): l'orario torna indietro.
Avviso K-CTR-035 (luoghi.json, /luoghi/5): Il luogo F-CASTELROVO-ALTO (Castelrovo, Alto) non ha le coordinate: serviranno al calcolo delle distanze.
Avviso K-CTR-043 (luoghi.json, /luoghi/17/trasbordo): Il punto di trasbordo L-TRASB-CROCE (Croce di Pradello, parcheggio) e' autorizzato solo a voce: manca il documento.
Avviso K-CTR-036: Il pacchetto non ha i tempi di sosta (file tempi_sosta.json): serviranno al calcolo.
Riepilogo: 2 aziende, 23 luoghi, 7 servizi, 10 corse; 1 errore, 3 avvisi.
```

Non sono in questo catalogo (arriveranno con la verifica delle regole, dopo il K-008): posti a bordo,
attese massime sulle corse, limiti di accesso confrontati con i mezzi, autisti e mezzi in due posti,
km calcolati, limiti di legge sugli orari di lavoro.

## Elenco completo

Questo elenco deve coincidere con `kzero descrivi`: una prova automatica
(`documentazione.errori_e_schemi`) controlla le prime tre colonne.
Un nuovo controllo prende il codice successivo all'ultimo.

| Codice | Livello | Descrizione | Quando succede | Cosa fare |
|---|---|---|---|---|
| K-CTR-001 | errore | Manca il manifesto | La cartella non contiene `pacchetto.json`. | Aggiungere il manifesto (vedi formato-dati.md). |
| K-CTR-002 | errore | File illeggibile | Un file del pacchetto c'e' ma non si riesce a leggere (permessi, disco). | Controllare i permessi del file e riprovare. |
| K-CTR-003 | errore | JSON non valido | Il file non e' JSON valido: virgola o parentesi mancante, virgolette non chiuse... Riga e colonna indicano dove il programma se ne accorge: l'errore e' li' o poco prima. | Correggere la scrittura del file alla riga indicata o subito prima. |
| K-CTR-004 | errore | Campo ripetuto | Lo stesso campo compare due volte nello stesso oggetto (il JSON lo permetterebbe, ma uno dei due valori andrebbe perso in silenzio). | Tenere un solo valore. |
| K-CTR-005 | errore | Schema del file sbagliato o mancante | Manca il campo `schema`, oppure dichiara uno schema diverso da quello del file (es. `luoghi.json` con lo schema `fermate`). | Scrivere lo schema giusto per quel file (tabella in formato-dati.md). |
| K-CTR-006 | errore | Versione dello schema non supportata | La `schema_versione` del file non e' tra quelle che questo programma sa leggere, oppure manca. Il file non viene letto, nemmeno in parte. | Usare un programma aggiornato, oppure scrivere il file nella versione indicata nel messaggio. |
| K-CTR-007 | errore | File non conforme allo schema | Il file non rispetta il suo schema: campo obbligatorio mancante, campo non previsto, valore non ammesso, forma sbagliata (es. orario `12.05`), numero fuori dai limiti. Il messaggio dice cosa e dove. Il valore sbagliato di un campo personale o particolare compare solo con il parametro che lo permette. | Correggere il campo indicato dal percorso JSON. |
| K-CTR-008 | avviso | File non riconosciuto | Nella cartella c'e' un file o una cartella che non fa parte del formato (spesso un nome scritto male, es. `fermate.json`). Non viene letto. | Rinominarlo con il nome giusto o toglierlo. |
| K-CTR-009 | errore | Codice doppio | Due elementi dello stesso tipo hanno lo stesso codice (i gruppi di passeggeri e i passeggeri in tutto il pacchetto). | Dare a ognuno un codice diverso. |
| K-CTR-010 | errore | Riferimento a un codice inesistente | Un dato nomina un codice che non esiste tra gli elementi di quel tipo (es. una corsa che passa da `F-PRADELO`). | Correggere il codice o aggiungere l'elemento mancante. |
| K-CTR-011 | errore | Luogo del tipo sbagliato | Una scuola o un centro sta in un luogo che non e' di tipo `scuola` o `centro`; una rimessa in un luogo che non e' `rimessa` o `ricovero_esterno`. | Correggere il luogo o il suo tipo. |
| K-CTR-012 | errore | Fine prima dell'inizio | Un periodo (manifesto, calendario, sospensione, orario di scuola, regolamento, contratto) finisce prima di cominciare, oppure un orario (lezioni, finestra di un gruppo) finisce prima di cominciare. | Scambiare o correggere le due date o i due orari. |
| K-CTR-013 | errore | Data fuori dal periodo del calendario | Una data aggiunta o tolta, o una sospensione, cade fuori dal periodo del suo calendario. | Correggere la data o allargare il periodo. |
| K-CTR-014 | errore | Data contraddittoria nel calendario | Nello stesso calendario una data e' sia aggiunta sia tolta, oppure una data aggiunta cade in una sospensione (le date tolte e le sospensioni prevalgono). | Decidere se in quel giorno il calendario vale, e scriverlo una volta sola. |
| K-CTR-015 | errore | Calendario senza nessun giorno | Il calendario non vale in nessun giorno del periodo del pacchetto. | Controllare giorni della settimana, periodo, date tolte, sospensioni e calendario di base. |
| K-CTR-016 | errore | Calendario basato su se stesso | Il calendario si basa su se stesso, direttamente o attraverso altri calendari (A ha base B, B ha base A). | Togliere la base che chiude il giro. |
| K-CTR-017 | errore | Orario che torna indietro in una corsa | Dentro una corsa un orario viene prima di quello del passaggio precedente (caso reale: `16:08` scritto tra `12:05` e `12:19`). Quando il passaggio dopo torna in ordine, il messaggio indica proprio l'orario fuori posto. Anche una partenza prima dell'arrivo nello stesso passaggio. | Correggere l'orario indicato (dopo mezzanotte si scrive 24:05, non 00:05). |
| K-CTR-018 | errore | Nome o codice esterno di fermata che vale per due fermate | Dopo la normalizzazione (maiuscole, accenti, punteggiatura, spazi) lo stesso nome, nome alternativo o codice esterno appartiene a due fermate: la ricerca non saprebbe quale scegliere. | Distinguere i nomi (es. aggiungere la via) o correggere il codice esterno. |
| K-CTR-019 | errore | Dati personali in un pacchetto "solo numeri" | Il pacchetto e' di livello `solo_numeri` ma contiene luoghi privati (case, punti porta a porta) o passeggeri uno per uno. | Togliere quei dati, oppure dichiarare il livello `completo`. |
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
