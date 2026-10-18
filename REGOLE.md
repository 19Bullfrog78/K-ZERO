# Regole del mestiere (confermate da Mauro)

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
  la "linea" del committente (es. la B634 di Trentino Trasporti) contiene piu' corse.
- Di solito un servizio si perde o si scambia una corsa alla volta, a volte un intero lotto (gruppo di servizi).
- Quattro macro categorie: Linee, Handicap (nel formato dei dati: disabili), Scolastici, Ibridi.
- Ibrido = qualsiasi mescolamento di gruppi sulla stessa corsa (studenti + handicap, studenti + linea, ecc.).
- Ibrido anche: studenti portati a un centro esterno (doposcuola, centro diurno).
- Una corsa che soddisfa piu' servizi e' ibrida. Una stessa corsa puo' servire committenti diversi.
  Esempio: a mezzogiorno si caricano le superiori insieme alle medie; le superiori sono un servizio a supporto
  di Trentino Trasporti (servizio pubblico della Provincia) e non compaiono nel file del CTA. La corsa e' ibrida.
- Corse ibride finora sempre autorizzate: infanzia + handicap, medie + superiori, medie + handicap.

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
- Lo sblocco si registra in un file separato; il file del CTA non si modifica mai.

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
- Pausa a casa (risposta di Mauro del 13/10): vale dai 30 minuti in su, se il giro lo consente. E' una
  preferenza, non un obbligo.
- Il rientro a casa o in rimessa dev'essere giustificato dai dati (tempi, km, costi). La regola precisa la fissa
  il mandato "orari di lavoro".
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
- Esempio di rotazione (Alpinabus, 11/10): ciclo di 14 giorni che parte di lunedi'; due turni del giorno,
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
- Esempio Alpinabus: le uscite di scuola di mezzogiorno (12.00-13.15) sempre allo stesso autista;
  le linee 13.00-14.30 sempre all'autista che abita piu' vicino.
- Se un'azienda tiene fisse solo alcune corse, le mette in un turno a parte.

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
- GSH = Gruppo Sensibilizzazione Handicap. I suoi centri sono piu' d'uno in zona; Alpinabus ne serve uno,
  quindi nei fogli "GSH" indica anche quel luogo.
- MITT = obliteratrice.
- Inserviente = ausiliaria = assistente a bordo.
- AM / RM / RP = andata mattina / ritorno mezzogiorno / ritorno pomeriggio (file del CTA).
- Lotto = gruppo di servizi affidati insieme con lo stesso contratto.
