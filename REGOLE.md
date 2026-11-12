# Regole del mestiere (confermate dal supervisore)

Le regole sono divise in due parti:
- **Parte 1 - Regole generali del mestiere**: valgono per ogni azienda e ogni territorio.
- **Parte 2 - Regolamento della Provincia di Trento (esempio)**: i valori di un territorio.
  Nel programma un regolamento e' un **dato** (file `regolamenti.json` del pacchetto), non una regola scritta
  nel codice: ogni contratto dice quale regolamento vale per i suoi servizi. Un altro territorio o un altro
  committente avra' il suo regolamento, con i suoi valori.

# Parte 1 - Regole generali del mestiere

## Servizi e corse
- **Servizio** = il lavoro che un committente affida all'azienda con un contratto: si paga, si perde, si rinuncia,
  si scambia. Ha un calendario e una **domanda**: chi deve andare da dove a dove ed entro quando.
- **Corsa** = il viaggio vero di un mezzo: una fila di passaggi (luogo, orario, chi sale e chi scende).
  Una corsa puo' soddisfare piu' servizi.
- La parola **"linea"** ha due sensi: la "linea" di un'azienda e' spesso la "corsa" del committente;
  la "linea" del committente (es. la B634 dell'azienda del trasporto pubblico provinciale) contiene piu' corse.
- Di solito un servizio si perde o si scambia una corsa alla volta, a volte un intero lotto (gruppo di servizi).
- Categorie (supervisore, 3/11): Linee, Handicap (nel formato dei dati: disabili), Scolastici, NCC (noleggio con conducente:
  gite, turismo), Promiscui (di solito scolastico + handicap; fino al K-011 "ibridi"), e "altro" per i servizi che non sono
  nessuno di questi (altre aziende, altri paesi). Il tipo del servizio lo dichiara il committente; la categoria della corsa
  la ricava il programma, non si scrive a mano.
- Promiscuo = qualsiasi mescolamento di gruppi sulla stessa corsa (studenti + handicap, studenti + linea, ecc.).
- Promiscuo anche: studenti portati a un centro esterno (doposcuola, centro diurno).
- Una corsa che soddisfa piu' servizi e' promiscua, anche se uno e' NCC. Una stessa corsa puo' servire committenti diversi.
  Esempio: a mezzogiorno si caricano le superiori insieme alle medie; le superiori sono un servizio a supporto
  dell'azienda del trasporto pubblico provinciale (servizio pubblico della Provincia) e non compaiono nel file del consorzio. La corsa e' promiscua.
- Corse promiscue finora sempre autorizzate: infanzia + handicap, medie + superiori, medie + handicap.
- **NCC** = servizi per gruppi formati su iniziativa del cliente o dell'azienda (gite, turismo, eventi, matrimoni), anche
  se si ripetono. Il tipo dice la natura del servizio, non la licenza del mezzo: un trasporto con tragitto e frequenza fissi
  per una categoria di persone (es. gli operai casa-lavoro) e' un servizio regolare specializzato anche se si fa con un mezzo
  da noleggio, e non e' NCC (va "altro", o il tipo giusto) [Probabile: Reg. (CE) 1073/2009 art. 2, stesse definizioni del
  Reg. (CE) 12/98 art. 2]. Senza questa regola una navetta di operai scritta NCC diventerebbe un servizio occasionale certo.
- Per un servizio NCC chi sale non cambia la categoria: una gita scolastica porta studenti ma e' NCC (supervisore mette le gite
  nell'NCC). Per gli altri tipi tutto come prima (gruppi misti = promiscuo).
- Il committente puo' essere un privato (es. un matrimonio): nome e nota sono dati personali (sotto, "Dati personali nel
  lavoro").

## Orari
- Linee: orari fissi, li decide il committente; si cambiano solo su sua richiesta e con largo anticipo.
- Scolastico e handicap: orari liberi dentro le regole del regolamento; i cambi grandi vanno concordati.
- Le tolleranze non ufficiali (es. "qualche minuto in piu' va bene") non entrano mai nel piano.
- Attesa a scuola prima e dopo le lezioni: il limite lo fissa il regolamento del territorio
  (es. Provincia di Trento, Parte 2); dentro quel limite l'attesa si concorda con la scuola.
- Gli orari cambiano per giorno della settimana e per periodi con date (di solito i due quadrimestri,
  ma esistono anche periodi brevi): il calcolo va fatto per ogni combinazione giorno + periodo.

## Alunni attivi e sospesi
- Attivi = hanno diritto al trasporto per legge: il piano si fa su di loro.
- Sospesi (passivi) = posti liberi: li sblocca la Provincia con una mail o con una lista non ufficiale.
- Lo sblocco si registra in un file separato; il file del consorzio non si modifica mai.

## Posti a bordo
- I posti sono quelli del libretto (carta di circolazione) e non cambiano con l'eta' dei passeggeri.
- Se i ragazzi superano i posti, il mezzo fa piu' giri, purche' stiano nell'orario della scuola.
- Il committente puo' chiedere un mezzo minimo (es. 30 posti) o la pedana, e puo' vietare un mezzo piu' grande.
- Un mezzo piu' grande del richiesto non e' pagato di piu': la differenza e' a spese dell'azienda.
- Carrozzina: chi viaggia seduto sulla carrozzina ha bisogno di un mezzo con pedana e di un posto carrozzina;
  ogni carrozzina toglie posti a sedere.
- Carrozzina piegata e altri ausili dichiarati (es. deambulatore): il passeggero siede su un posto normale e la
  pedana non serve. L'ausilio piegato va nel bagagliaio: nell'auto nel baule, nel minibus con il vano bagagli
  nel vano. Nel minibus senza vano va tra i sedili, legato e bloccato, e occupa dei posti (es. 2), solo se sono
  liberi: se il mezzo e' pieno, il passeggero con l'ausilio non e' ammesso.
- "Tra i sedili" e' una scelta facoltativa dell'azienda, che ne risponde: vale solo sui mezzi per cui l'azienda
  l'ha indicata.
- Gli ausili si contano, non si misurano: un deambulatore conta come una carrozzina piegata.
- Il passeggino non si dichiara e non si pianifica.
- Salita e discesa dei passeggeri handicap richiedono molto piu' tempo; si aggiunge il tempo della pedana a ogni fermata.

## Mezzi e strade
- Un mezzo non passa ovunque: limiti di misure e di peso, sottopassi, piazzali stretti, sbalzo posteriore.
- Conta anche l'esperienza dell'autista: alcuni passaggi si evitano anche dove non c'e' un cartello.
- I mezzi non sempre rientrano in rimessa: possono dormire in ricoveri esterni vicini a casa dell'autista.
- Costo del mezzo = quota fissa giornaliera + costo al km.
- Un mezzo fa solo i servizi per cui e' omologato: la carta di circolazione dice l'uso del mezzo (in Italia
  Codice della strada art. 82: uso proprio oppure uso di terzi, che comprende il noleggio con conducente e il
  servizio di linea). Usarlo per un uso diverso e' sanzionato (art. 82 c. 8; art. 87 c. 6-7).
- Per l'altro uso serve la "distrazione": un'autorizzazione da tenere a bordo con la carta di circolazione
  (art. 87 c. 4; circolare del Ministero dei trasporti n. 27690 del 27/11/2013), per singolo mezzo e con le date
  (es. rinnovata ogni anno). Chi la rilascia cambia secondo il verso (da linea a noleggio o il contrario) e il
  territorio (ente che concede la linea oppure Motorizzazione): l'ente e' un dato, mai una regola nel programma.
- Alcuni mezzi hanno un vincolo di destinazione (es. comprati con contributi pubblici per le linee: niente
  noleggio, fino a una data): anche questo e' un dato del mezzo, con la sua fonte.
- Quali usi servono per ogni tipo di servizio e' un dato con la fonte (norme del paese, regolamento del
  territorio), mai una regola scritta nel programma.

## Trasbordi
- Solo in un punto autorizzato dal committente.
- Il secondo mezzo e' gia' sul posto quando arriva il primo: l'attesa del passeggero non si pianifica.
- Un minore o un disabile non resta mai solo.
- Ammesso anche tra aziende diverse, se concordato.
- Gli imprevisti non si pianificano.

## Assistente a bordo
- Quando serve lo dice il regolamento o il contratto: in alcuni appalti solo per l'infanzia, in altri sempre.
  Serve anche per i passeggeri handicap che lo richiedono.
- Occupa un posto.
- Chi lo fornisce dipende dal contratto: di solito Provincia/Comune/associazione, a volte l'azienda.
- Se e' dell'azienda va pianificato e pagato come un autista.

## Autisti
- La pausa fuori dal posto (rientro in rimessa o in un ricovero tra due corse) si giudica con la chiave del rientro
  dell'azienda e della persona: vedi "Orari di lavoro" qui sotto (supervisore, 20/10). Resta una preferenza, non un
  obbligo, se l'azienda non la fa diventare un limite.
- Il mezzo dorme preferibilmente in un ricovero vicino a casa dell'autista, cosi' l'autista usa la sua auto il
  meno possibile. Altre aziende vogliono sempre il rientro in rimessa: e' un'impostazione di ogni azienda
  (ricovero vicino a casa preferito, ammesso, mai).
- Abbinamento autista-mezzo: anche questo lo sceglie ogni azienda (nessuno, preferito, fisso = ogni autista ha
  il suo mezzo).

## Turni del giorno e rotazione degli autisti
- Ogni azienda organizza turni e persone a modo suo: il programma registra e controlla l'organizzazione scritta
  dall'azienda, non decide niente da solo, e ogni scelta dell'utente vince.
- Turno del giorno: il lavoro di una persona in un giorno (mezzo, corse, attivita' senza passeggeri, luogo di
  inizio e di fine). I nomi dei turni li sceglie l'azienda (es. "primo", "secondo").
- Turni spezzati (mattina e pomeriggio), turni notturni oltre la mezzanotte, cambio dell'autista lungo una corsa
  e cambio del mezzo nello stesso giorno sono turni normali.
- Chi fa il turno: tre modi, dal piu' generale al piu' preciso. Vince il piu' preciso, turno per turno:
  1. rotazione: ogni azienda sceglie la sua (es. un ciclo di 3 giorni, di 8 giorni con 6 di lavoro e 2 di
     riposo, di 14 giorni, di 4 settimane, di un mese, oppure nessuna rotazione) e il suo modo di assegnare i
     turni (rotazione, turni fissi, scelta per anzianita', a mano). Il ciclo riparte da capo e continua anche
     durante le vacanze. Il riposo puo' scorrere, anche in un servizio 7 giorni su 7;
  2. servizi fissi: un turno sempre alla stessa persona, con i suoi giorni (es. solo il mercoledi'); gli altri
     giorni il turno segue la rotazione;
  3. assegnazione del singolo giorno: sostituzioni, scambi tra colleghi, un mezzo al posto di quello in
     revisione, lavori occasionali (gite, noleggi). Le scrive l'utente: il programma non le pianifica.
- Se nessuno dei tre modi da' una persona, il turno e' scoperto. Il programma non inventa assegnazioni.
- I cambi occasionali per impegni personali non si pianificano.
- Estate: il lavoro diminuisce e gli autisti vanno in ferie; corse, turni e rotazione possono quindi cambiare
  da un periodo dell'anno all'altro (piu' rotazioni nello stesso anno, anche per gruppi diversi di persone).
- Esempio di rotazione (ALP, 11/10): ciclo di 14 giorni che parte di lunedi'; due turni del giorno,
  "primo" (dalla prima linea del mattino a verso le 17.00) e "secondo" (dalle 8.00 o piu' tardi a verso le 20.30
  con l'ultima linea); il sabato un solo autista fa tutto il giorno (con 1h30 di pulizia del mezzo); la domenica
  nessuno. Il secondo autista fa sempre l'opposto del primo.

  | Giorno del ciclo | Autista A | Autista B |
  |---|---|---|
  | 1 lunedi' | primo | secondo |
  | 2 martedi' | secondo | primo |
  | 3 mercoledi' | secondo | primo |
  | 4 giovedi' | primo | secondo |
  | 5 venerdi' | primo | secondo |
  | 6 sabato | riposo | tutto il giorno |
  | 7 domenica | riposo | riposo |
  | 8 lunedi' | secondo | primo |
  | 9 martedi' | primo | secondo |
  | 10 mercoledi' | primo | secondo |
  | 11 giovedi' | secondo | primo |
  | 12 venerdi' | secondo | primo |
  | 13 sabato | tutto il giorno | riposo |
  | 14 domenica | riposo | riposo |

  Altre aziende cambiano turno ogni giorno, ogni settimana, ogni mese, oppure mai per tutto l'anno scolastico.

## Servizi fissi
- Alcuni servizi restano sempre allo stesso autista, fuori dalla rotazione, per ridurre km a vuoto e ore di lavoro.
  Hanno i loro giorni (es. solo il mercoledi'); gli altri giorni il turno segue la rotazione.
- Esempio ALP: le uscite di scuola di mezzogiorno (12.00-13.15) sempre allo stesso autista;
  le linee 13.00-14.30 sempre all'autista che abita piu' vicino.
- Se un'azienda tiene fisse solo alcune corse, le mette in un turno a parte.

## Orari di lavoro (K-011)
Il programma conosce solo i **tipi di regola** (lavoro massimo, guida di fila, riposo giornaliero, ...): i numeri, le
fonti e quando una regola vale stanno nei dati. Senza dati non dice mai "regolare".

### Tre livelli
1. **La legge** (e il contratto collettivo): nelle norme del paese (`norme.json`), regime per regime, con la fonte.
   Una violazione certa (lettura certa della legge, regime che vale con certezza) e' un **errore**; una violazione
   solo con la lettura prudente, o con qualcosa da confermare (il rapporto, il tipo di servizio, i km...), e' un
   **avviso** che dice perche'.
2. **Il contratto della persona** (`persone.json`): ore previste (si mostrano con la differenza) e ore massime
   (limite rigido: superarle e' un errore). Le ore si contano come le conta il regime che vale quel giorno (lavoro
   effettivo della L. 138, orario di lavoro del D.Lgs. 234); senza nessun regime: guida, altre mansioni e
   disponibilita' sul posto.
3. **Le scelte dell'azienda e della persona** (`aziende.json`, `persone.json`): lavoro massimo, guida massima,
   ampiezza della giornata, riposo minimo tra due giornate, giorni di fila, la chiave del rientro. Ognuna e' una
   preferenza (avviso) o un limite rigido (errore su un turno ricorrente, avviso su un'assegnazione del singolo
   giorno: la scelta dell'utente vince). La scelta della persona vince su quella dell'azienda. La legge resta sempre
   sopra: una scelta piu' larga della legge si accetta con un avviso che lo dice.

### Categorie di tempo
Ogni minuto di un turno ha una categoria: guida, altre mansioni, disponibilita' (pronto a partire, a disposizione),
reperibilita' (da casa, solo raggiungibile), interruzione (libero), riposo. Le corse sono guida; le attivita' hanno
la categoria scritta dall'azienda; le soste non scritte tra due elementi prendono la categoria scelta dall'azienda
(Alfa: fino a 30 minuti disponibilita', oltre interruzione; Beta: fino a 15). Ogni legge dice poi quali categorie
conta come lavoro (es. la L. 138 conta la disponibilita', il D.Lgs. 234 no).
Nel demo le categorie dubbie hanno il motivo scritto accanto: ATTESA e SOSTA con il mezzo sono disponibilita' (se il
conducente e' libero si scrive una pausa); RICARICA e RISERVA disponibilita'; REPERIBILE reperibilita'; A-PIEDI
(spostamento senza il mezzo) altre mansioni.

### La pausa fuori dal posto (chiave del rientro, supervisore 20/10)
- Rientro = tra due corse della giornata la persona va in una rimessa o in un ricovero, resta li' e torna. Una pausa
  dove finisce la corsa e' "sul posto": la chiave non la riguarda.
- La chiave e' dell'azienda, con valori diversi per persona: fermo minimo (minuti), fattore della guida, km a vuoto
  in piu' al massimo (facoltativo), forza (preferenza o limite).
- Il rientro e' giustificato se: il tempo fermo e' almeno il fermo minimo; il tempo fermo e' almeno il fattore per i
  minuti di guida a vuoto in piu'; i km a vuoto in piu' non superano il massimo (se c'e').
- "In piu'" = i trasferimenti verso il luogo e indietro, meno il trasferimento diretto che servirebbe comunque: zero
  se la corsa dopo comincia dove finisce quella prima. Senza le distanze il programma giudica solo quando e' certo
  (anche contando tutti i trasferimenti come "in piu'"); altrimenti "non giudicato: servono le distanze", mai
  "giustificato".
- Principio del 20/10: un fermo minimo piu' basso delle soste che la legge conta come lavoro (L. 138 art. 6: 30
  minuti) non e' vietato, ma quel tempo resta lavoro pagato: il programma lo dice con un avviso.
- Demo: Alfa 30 minuti, fattore 2, preferenza; Beta 45 minuti, fattore 1,5, al massimo 15 km in piu', limite
  rigido; la persona P-E ha il suo fermo minimo di 45 minuti.
- Gli esempi del supervisore (pacchetto di prova tests/esempi/k011-orari): (a) 20 + 20 minuti di guida per 30 minuti fermo:
  non giustificato (servono 80 minuti); (b) 15 minuti fermo: non giustificato (sotto i 30); (c) 2 ore e 16 fermo
  contro 36 minuti di guida in piu' (ne servono 72): giustificato.

### OUT o tachigrafo (supervisore 28/10)
- Dove la legge lo permette (linee e scolastico fino a 50 km, fuori dal Reg. 561) l'azienda sceglie: **OUT** (regole
  nazionali, la L. 138) oppure il **tachigrafo** (segue per scelta il Reg. 561 e il D.Lgs. 234). E' una scelta
  dell'azienda, per tipo di servizio, per servizio o per corsa: vince la piu' precisa.
- Il tachigrafo e' una scelta con la sua forza, come le altre scelte dell'azienda: preferenza (avviso) o limite
  (errore su un turno ricorrente, avviso su un'assegnazione del singolo giorno); vale anche l'estensione alla
  settimana. La L. 138 resta sopra comunque.
- OUT dove la legge vuole il 561 (es. una linea di 50,1 km): vince la legge, con un avviso.
- Una corsa con servizi di tipi diversi e scelte diverse: decide l'impostazione dell'azienda (di base la piu' prudente,
  il tachigrafo), con un avviso che suggerisce di scrivere la scelta per la corsa.
- **Dichiarazione**: dove l'esclusione dal 561 e' da confermare (oggi: il servizio solo per disabili), l'azienda puo'
  dichiararlo escluso, con motivo, chi e quando: da li' e' escluso con certezza, con una nota sola.
- **Forzatura cosciente**: l'azienda puo' scegliere OUT anche dove, con i dati scritti, vale il 561 (es. i km scritti
  sono sbagliati), con motivo, chi, quando ed eventualmente fino a quando. Il programma pianifica come ha scelto
  l'azienda, ma la forzatura si vede sempre (kzero controlla, giornata e ore) e il rischio e' un avviso "forzatura
  dell'azienda" con le violazioni che ci sarebbero se fosse sbagliata.
- Il consulente (riferito dal supervisore il 28/10): lo scolastico fino a 50 km e' OUT con certezza, anche con passeggeri
  disabili sulla stessa corsa; il servizio solo per disabili resta da confermare.
- Demo: Alfa linee e scolastico OUT, NCC (gite e noleggi; fino al K-011 tipo "altro") con il tachigrafo; Beta linee con il tachigrafo, come
  preferenza. Nel pacchetto di prova una forzatura (corsa C-FORZATA di 55 km, "il percorso vero e' di 48 km") e una
  dichiarazione (servizio S-DIS-GIO, solo disabili).

### NCC e leggi sugli orari (K-012)
- Reg. 561: l'NCC e' un servizio occasionale, mai escluso; vale con un mezzo che porta piu' di 9 persone compreso il
  conducente [Probabile: Reg. (CE) 1073/2009 art. 2, occasionali = non regolari]. Per i servizi occasionali l'interruzione di
  45 minuti si puo' fare anche in due parti di 15 (art. 7): nel demo per l'NCC con certezza, per "altro" da confermare (un
  avviso, mai un errore). Un trasferimento a vuoto vale come la corsa a cui serve (come per la guida delle corse escluse).
- L. 138: non vale per l'NCC (e' per le linee). D.Lgs. 234: segue il 561, come per gli altri servizi.
- NCC con un'auto fino a 9 persone: niente 561; per i dipendenti nessuna legge sugli orari nei dati (avviso K-CTR-107,
  domanda 17 qui sotto).
- Usi dei mezzi: l'NCC chiede l'uso NCC, con certezza (Codice della strada art. 82) [Probabile]; un servizio "altro" l'uso NCC
  solo da confermare. Un autobus di linea fa l'NCC con la distrazione (nel demo ALFA-05, fino al 30/06/2027).
- Gita scolastica: e' NCC anche se porta studenti, quindi niente esclusione dello scolastico fino a 50 km dal 561.
- Nel demo Alfa sceglie il tachigrafo per l'NCC (prima per il tipo "altro"); dove vale la legge, vale la legge.

### Le leggi del demo (norme N-IT, regole pubbliche con la fonte)
Riassunti per il pacchetto dimostrativo (letture dell'arbitro del 20/10). "dc" = da confermare (vedi l'elenco sotto).

| Regime | Fonte | Quando vale | Regole |
|---|---|---|---|
| R-L138 | L. 14 febbraio 1958, n. 138 | autisti; dipendenti (tempo pieno, part-time, determinato, stagionale, a chiamata); servizi di linea (non l'NCC). Dc: titolare, socio, collaboratore familiare, autonomo, volontario, altro; scolastico e disabili; assistente. Nelle settimane del 561 non vale per guida e riposi; per il lavoro resta (dc) | art. 6 lavoro effettivo (guida, altre mansioni, disponibilita', soste fino a 30 minuti, 12% dell'inoperoso fuori residenza); artt. 2-3: 8 ore al giorno e 48 a settimana, con lo straordinario al massimo 10 e 60; art. 4 notte 22-5 (solo conteggio); art. 5 guida di fila al massimo 5 ore; art. 7 riposo di 9 ore nelle 24 ore dall'inizio della giornata; art. 8 riposo settimanale di 24 ore |
| R-561 | Reg. (CE) n. 561/2006, testo consolidato al 31/12/2024; nota INL n. 61 del 14/01/2021 | chi guida un mezzo che porta piu' di 9 persone compreso il conducente; esclusi i servizi di linea fino a 50 km (art. 3 a); scolastico fino a 50 km escluso, anche con disabili sulla stessa corsa (il consulente, supervisore 28/10); solo disabili fino a 50 km escluso solo dc (art. 4 n); servizi occasionali mai esclusi: nel demo l'NCC con certezza (K-012), il tipo "altro" dc. Una corsa che lo fa valere lo porta su tutta la settimana per guida e riposi (nota INL). La guida delle corse escluse conta come altre mansioni per le sue regole di guida (art. 6 c. 5; dc, prudente: guida) | art. 6 guida 9 ore tra due riposi (10 due volte a settimana), 56 a settimana, 90 in due settimane; art. 7 dopo 4 ore e 30 di guida 45 minuti di interruzione (o 15 + 30; 15 + 15 per i servizi occasionali); art. 8 riposo giornaliero 11 ore (3 + 9; ridotto 9, al massimo 3 volte), riposo settimanale 45 ore (ridotto 24) entro 6 periodi di 24 ore, compensazione entro la terza settimana |
| R-234 | D.Lgs. 19 novembre 2007, n. 234 | dove vale il 561, per dipendenti e autonomi (nelle settimane in cui il 561 vale per estensione, nei giorni senza le sue corse dc); dc: titolare, socio, collaboratore familiare, volontario, altro, assistente | art. 3 orario di lavoro = guida e altre mansioni; art. 4 al massimo 60 ore a settimana e media 48 su 4 mesi; art. 5 mai piu' di 6 ore di fila senza pausa, 30 minuti tra 6 e 9 ore, 45 oltre; art. 7 con lavoro notturno al massimo 10 ore in 24 |
| R-CHIAMATA | D.Lgs. 15 giugno 2015, n. 81, art. 13 c. 3 | rapporto a chiamata | al massimo 400 giornate in tre anni con lo stesso datore (solo avvisi: oltre e vicino al limite) |
| ESEMPIO-01 | contratto collettivo inventato | chi ha quel contratto (P-A, e P-CONTRATTO nel pacchetto di prova) | regole inventate per provare i contratti collettivi: ampiezza 14 ore, riposo 10 ore tra due giornate, 7 giorni di fila |

Il cambio dell'ora (ora legale) e' nei dati del paese (`cambi_ora`, Direttiva 2000/84/CE): un turno che lo attraversa
da' un avviso, perche' le durate vanno controllate a mano.

### Da confermare (da portare al consulente del lavoro)
Il programma le tratta oggi con prudenza: con la lettura certa da' errori, con quella prudente solo avvisi.
1. L. 138: vale anche per titolare, socio, collaboratore familiare, autonomo, volontario e "altro"?
2. L. 138: vale anche per i servizi scolastici e per disabili (non di linea)? E per l'assistente a bordo?
3. L. 138 artt. 2-3: la settimana va da lunedi' a domenica (come nel D.Lgs. 234)?
4. L. 138 art. 3: lo straordinario su un turno che si ripete ogni settimana e' "non meramente saltuario" (vietato
   salvo speciali esigenze dell'azienda)?
5. L. 138 art. 5: nelle 5 ore di guida di fila si contano anche le prime due interruzioni fino a 30 minuti, e solo
   un intervallo di almeno 1 ora azzera il conto? L'eccezione del c. 4 (soste ai capolinea oltre 15 minuti) vale
   per le nostre linee?
6. L. 138 art. 6: "fuori residenza" vuol dire fuori dal comune della presa servizio? La reperibilita' da casa e' lavoro
   effettivo? Il 12% del tempo inoperoso vale anche nel comune della presa servizio?
7. L. 138 art. 7: la media di 11 ore si conta su ogni 28 giorni di fila? La riduzione a 9 ore con le interruzioni
   (una di 2 ore o due di 1 ora) toglie quelle giornate dalla media?
8. L. 138 art. 8: il riposo settimanale e' 24 ore piu' il riposo giornaliero (33 ore)? Il cumulo di due riposi in
   due settimane e' ammesso da noi (esigenze di servizio o accordo)?
9. L. 138 nelle settimane in cui vale il 561: i limiti del lavoro effettivo (artt. 2-3) restano?
10. Reg. 561: lo scolastico fino a 50 km e' escluso come le linee (art. 4 n, dal Reg. 2024/1258), anche con passeggeri
    disabili sulla stessa corsa: confermato dal consulente (riferito dal supervisore il 28/10). Resta da confermare: un servizio
    solo per disabili fino a 50 km e' un "servizio regolare specializzato" escluso? Fino alla risposta l'azienda puo'
    dichiararlo escluso nei dati (dichiarazione, "OUT o tachigrafo").
11. Reg. 561: quali nostri servizi sono "servizi occasionali"? Dal K-012 (supervisore, 3/11) l'NCC (gite, turismo, eventi,
    matrimoni: gruppi formati su iniziativa del cliente o dell'azienda, anche se si ripetono) lo e' con certezza nel demo
    [Probabile: Reg. (CE) 1073/2009 art. 2, occasionali = non regolari ne' regolari specializzati]; resta da confermare il
    confine con il servizio regolare specializzato (es. una navetta di operai con tragitto e orari fissi fatta con un mezzo
    da noleggio: per noi "altro", e un servizio "altro" resta occasionale solo da confermare). Un NCC con un'auto fino a 9
    persone (conducente compreso) e' fuori dal 561: per i dipendenti quale legge vale e' la domanda 17.
12. D.Lgs. 234: titolare, socio e collaboratore familiare sono "autotrasportatori autonomi"? L'assistente e' un
    "lavoratore mobile"? E il volontario?
13. D.Lgs. 234 art. 4: la media di 48 ore si conta su ogni 4 mesi di fila?
14. D.Lgs. 234 art. 7: quali 4 ore tra le 00:00 e le 07:00 sono la "notte" (il programma conta certo il lavoro tra
    le 03:00 e le 04:00, prudente qualunque lavoro tra le 00:00 e le 07:00)?
15. D.Lgs. 81/2015 art. 13: i tre anni si contano a ritroso dal giorno (triennio mobile)? Le giornate prima del
    pacchetto sono un dato approssimato della persona.
16. Le categorie delle attivita' del demo (ATTESA, SOSTA, RICARICA, RISERVA come disponibilita'; A-PIEDI come altre
    mansioni; REPERIBILE come reperibilita') e le soste non scritte (Alfa: oltre 30 minuti libere; Beta: oltre 15).
17. Per i dipendenti a cui non vale la L. 138 (es. servizi non di linea fuori dal 561): quale legge sugli orari si
    applica? Oggi il programma lo dice con un avviso e non giudica quegli orari.
18. Il contratto collettivo vero dell'azienda (es. quello applicato da ALP): va scritto nei dati come regime,
    con le sue regole, quando arriva dal consulente.
19. Nelle settimane in cui il Reg. 561 vale per estensione (una corsa sopra i 50 km o una gita), il D.Lgs. 234 vale per
    tutta la settimana o solo nei giorni di quelle corse? (Oggi: negli altri giorni da confermare.)
20. Nelle settimane in cui vale il Reg. 561, la guida delle corse di linea fino a 50 km conta nei limiti di guida del 561
    o e' "altre mansioni" (art. 6 c. 5)? (Oggi: altre mansioni nella lettura certa, guida in quella prudente, con un
    avviso.)
21. Se l'azienda registra con il tachigrafo corse che potrebbe fare in OUT, in un controllo vale il Reg. 561 o la L. 138?
    (Oggi: per il programma e' una scelta dell'azienda, con la sua forza; la L. 138 resta sopra.)

## Dati personali nel lavoro
- Chi pianifica vede tutto, nell'interfaccia per persone.
- Tre classi di dati:
  - comuni: fermate pubbliche, scuole e centri, orari, calendari, conteggi senza esigenze, mezzi, aziende,
    committenti, contratti, regolamenti;
  - personali: nomi, indirizzi, case, punti di salita porta a porta, motivi scritti a mano, ricoveri presso
    la casa di una persona;
  - particolari (salute): carrozzina (seduto o piegata), ausili piegati, assistente necessario, servizio per
    disabili, conteggi di carrozzine e di ausili.
- Due livelli di pacchetto dati: "solo numeri" (solo conteggi per fermata: e' il livello consigliato)
  e "completo" (anche i passeggeri uno per uno).
- Mai codice fiscale, telefoni, mail delle famiglie: per pianificare non servono.
- Clienti privati (K-012, supervisore 3/11): un committente puo' essere una persona (es. un matrimonio). Il nome e la nota sono
  dati personali, nella cassaforte; per un ente, una scuola o un'agenzia restano comuni. Il nome del cliente privato non si
  perde mai: il titolare e i delegati con i permessi vedono sempre nome e cognome (se il servizio va in un luogo sensibile,
  es. un centro diurno, servono tutti e due i permessi, come per i passeggeri); gli altri vedono il codice. Il codice del
  cliente e i nomi dei suoi servizi e contratti sono dati comuni: non devono contenere il nome (es. "Noleggio per un
  matrimonio", non "Matrimonio Rossi"). Mai telefono, mail o codice fiscale.
- Nomi liberi (K-018, supervisore 12/11): nomi, codici e note scritti da chi prepara i dati sono dati comuni e li vede chiunque.
  Il programma avvisa (solo avvisi) quando contengono il nome di una persona dei dati o una parola sulla salute degli elenchi
  del paese e dell'azienda; un falso allarme si spegne con un motivo scritto. Niente controllo sui numeri dei codici esterni.

## Da confermare
- Valori reali: tempo massimo a bordo, tempi di salita, tempo pedana, posti persi per carrozzina, costi.

# Parte 2 - Regolamento della Provincia di Trento (esempio)

Fonte: testo unico sul trasporto scolastico, delibera della Giunta provinciale n. 2 del 10/01/2025
(tra parentesi il punto del testo). Sono regole pubbliche. Nel pacchetto di esempio (`esempi/demo`)
sono scritte come regolamento, con la fonte di ogni valore.

- Attesa massima: 25 minuti prima dell'inizio e 20 minuti dopo la fine delle lezioni (1.B.1).
- Distanze massime: casa-fermata 1.000 m, fermata-scuola 500 m (1.B.1).
- Distanza minima casa-scuola: 500 m per l'infanzia, 1.000 m per primaria e secondaria di primo grado (1.B.1).
- Fermate di norma ad almeno 300 m l'una dall'altra (1.C1.5).
- Almeno 3 bambini per un servizio dell'infanzia (1.C1.6).
- Infanzia: accompagnatore a bordo sui mezzi con piu' di 9 posti (1.C1.9; DM 31/01/1997)
  e adulto sempre alla fermata (1.C1.12).
- Primaria e secondaria di primo grado: discesa senza adulto ammessa con l'autorizzazione dei genitori
  (1.B.4; art. 19-bis D.L. 148/2017, convertito dalla legge 172/2017).
- Passeggeri in piedi: mai per l'infanzia, e mai oltre 10 minuti o 5 km (1.B.6).
- Servizi di terapia: adulto all'arrivo a casa (2.C2).
- La sorveglianza durante l'attesa spetta alla scuola ed e' lavoro straordinario pagato: l'attesa va
  ridotta, giustificata e concordata (1.B.4, 1.C2.7, 1.C3.7).
- Le proposte di modifica degli orari scolastici vanno al Servizio mobilita' pubblica entro il 28 febbraio
  per l'anno scolastico successivo.

## Da confermare (Provincia di Trento)
- Infanzia e medie sulla stessa corsa: il testo unico non ne parla.
- "Mezzi con piu' di 9 posti" (1.C1.9): nel pacchetto di esempio e' letto come 9 posti compreso il conducente
  (come per gli autobus nel Codice della strada, art. 54), cioe' piu' di 8 posti per i passeggeri.

# Glossario
- GSH = Gruppo Sensibilizzazione Handicap. I suoi centri sono piu' d'uno in zona; ALP ne serve uno,
  quindi nei fogli "GSH" indica anche quel luogo.
- MITT = obliteratrice.
- Inserviente = ausiliaria = assistente a bordo.
- AM / RM / RP = andata mattina / ritorno mezzogiorno / ritorno pomeriggio (file del consorzio).
- Lotto = gruppo di servizi affidati insieme con lo stesso contratto.
