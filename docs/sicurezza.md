# Sicurezza e protezione dei dati in K-Zero

K-Zero trattera' dati di minori e dati sulla salute (persone con disabilita'), cioe' dati "particolari" (GDPR art. 9).
Mauro, 12/10 e 18/10: "il software dev'essere blindato"; "creare un software fallace dal punto di vista privacy e
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
ricevono dal CTA senza un accordo scritto noto. K-Zero quindi si progetta per il caso piu' stretto: **nessun dato
personale esce dal PC** di chi pianifica. Il programma lo potra' usare chiunque, anche in aziende con piu' persone.

## 1. Cosa tratta K-Zero, dove e perche' (base per la valutazione d'impatto, art. 35)

La valutazione d'impatto la fa il titolare; questa sezione gli da' i fatti sul programma.

| Domanda | Risposta |
|---|---|
| Perche' (finalita') | Pianificare il trasporto: servizi di linea, scolastici, per disabili, ibridi; turni di autisti e assistenti; mezzi. |
| Quali dati | Comuni: fermate pubbliche, scuole e centri, orari, calendari, mezzi, contratti, regolamenti, conteggi. Personali: dei passeggeri nome, casa, punto porta a porta, motivi scritti a mano (solo nel livello "completo"); di autisti e assistenti nome, casa, ruoli, rapporto e ore, disponibilita', abilitazioni, autorizzazioni, competenze, preferenze, assenze (tipo e giorni, mai la causa). Particolari (salute): esigenze dei passeggeri (carrozzina, ausili piegati, assistente necessario), tipo "disabili" di servizi e gruppi, conteggi di carrozzine e ausili, competenze richieste da un servizio o da un turno. Elenco completo, campo per campo: [formato-dati.md](formato-dati.md). |
| Di chi | Alunni (minori), persone con disabilita' e loro accompagnatori, autisti e assistenti delle aziende. |
| Quali dati mai | Codice fiscale, telefoni, mail, data di nascita, idoneita' medica, cause delle assenze: il formato non ha campi per scriverli (un campo cosi' aggiunto a mano e' un errore). |
| Quanto | Il minimo: il livello "solo numeri" (consigliato) ha solo conteggi per fermata, niente nomi, case, porta a porta, motivi. |
| Dove | Solo nei file del pacchetto (una cartella) sul PC di chi pianifica. Il programma non usa la rete, non scrive file senza `--scrivi`, non tiene archivi o registri propri. |
| Chi li vede | Chi usa il PC e apre i file. Le uscite del programma di base non contengono dati personali ne' particolari (sezione 3). |
| Per quanto | K-Zero non conserva nulla per conto suo: i tempi li decide il titolare per i file del pacchetto. |
| Chi decide | Titolare dei dati degli alunni: con ogni probabilita' la Provincia (da confermare con il suo RPD). L'azienda che pianifica per conto del committente e' responsabile del trattamento (art. 28) per quei dati, e titolare dei dati dei propri dipendenti. |

`kzero controlla <cartella>` dice in ogni momento quali dati contiene un pacchetto: il livello e quanti valori
personali e particolari ci sono (file per file con `--includi-dati-personali`).

## 2. Da cosa protegge, e da chi

| Chi o cosa | Cosa fa K-Zero oggi | Cosa resta |
|---|---|---|
| **Chi usa lo stesso PC** (un collega, un familiare) | Niente di proprio: i file del pacchetto sono file normali. | Utenti separati del sistema operativo (sezione 5). La cassaforte cifrata dei dati personali con password, piu' utenti e registro degli accessi arrivano con il mandato K-010. |
| **Un PC o una chiavetta persi o rubati** | Niente di proprio: chi ha i file li puo' leggere. | Cifratura del disco del sistema operativo (sezione 5); mandato K-010. |
| **Un file di dati preparato apposta**, o rovinato | Lettura blindata, limiti di buon senso, controllo con gli schemi, file malformati rifiutati senza blocchi; provato con i sanitizer e con il fuzzing a ogni proposta; protezioni del compilatore (sezione 3). | Rischi residui in sezione 4. |
| **Un assistente AI che legge le uscite** (spesso su server esterni) | Di base le uscite non contengono dati personali (al loro posto il codice) ne' particolari ("riservato"), nemmeno nei messaggi, nelle posizioni o nel solo fatto che ci sia un problema sulla salute (sezione 3). Un assistente AI e' solo un aiuto facoltativo. | Con `--includi-dati-personali` o `--includi-dati-particolari` l'uscita contiene quei dati: non va data a un assistente AI. |
| **Uno sbaglio di chi scrive i dati** | I controlli K-CTR (docs/controlli.md): es. un pacchetto "solo numeri" con dei nomi e' un errore (K-CTR-019). | I nomi liberi (servizi, corse, turni, note, codici) possono rivelare un dato sulla salute: sezione 5. |
| **La rete** | Nessun comando usa la rete; due prove lo controllano (sezione 3). | Il futuro adattatore delle distanze (es. Google Maps) sara' l'unica eccezione: un programma separato, acceso con un parametro esplicito, che manda fuori solo coordinate (mai nomi o altri dati delle persone), con un suo mandato. |
| **Un componente esterno compromesso** | Solo quattro librerie, con versione e impronta fissate; elenco dei componenti in un formato standard (sezione 3). | Le librerie del sistema (libreria C e C++ del compilatore) non sono nell'elenco. |

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
- **luoghi sensibili** (es. un centro diurno, segnato da chi scrive i dati): con i dati personali e senza quelli
  particolari, dove un'uscita lascia riconoscere un passeggero - il nome o la casa (un punto porta a porta con il nome o
  l'indirizzo) - il luogo sensibile diventa "riservato": nel messaggio di un problema, in servizi e corse della
  dimostrazione, in tutta la giornata mostrata ([formato-dati.md](formato-dati.md), "Luoghi sensibili"). Una regola
  sola nel codice (`LegameSensibile`, src/api/riservatezza.hpp), usata da tutte le uscite;
- gli errori del programma (K-ERR) non nominano mai un file di soli dati personali.

**Minimizzazione.** Nessun campo per codice fiscale, telefoni, mail, data di nascita, cause delle assenze, idoneita'
medica. Il livello "solo numeri" e' quello consigliato.

**Niente rete, provato.** Una prova legge il programma compilato (Linux) e controlla che non importi nessuna funzione di
rete (socket, connect, getaddrinfo e simili) e non sia collegato a librerie di rete; un'altra controlla che nessun file
del codice includa intestazioni di rete o si colleghi a librerie di rete.

**Niente scritture senza `--scrivi`.** Il risultato va sull'uscita standard; nessun comando scrive file.

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
- **fuzzing** (prove con dati casuali e deformati, libFuzzer): 27 bersagli, cioe' ogni tipo di file del pacchetto (il demo
  con quel file sostituito: lettura, controlli, messaggi), la riga di comando, date e orari; partono dai file del demo e
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

**Componenti esterni fissati ed elencati.** CLI11, nlohmann/json, valijson e (solo per le prove) GoogleTest e
check-jsonschema, con versione e impronta fissate. L'elenco completo, leggibile da un programma, e' in
[componenti.cdx.json](componenti.cdx.json) (formato standard CycloneDX 1.5): nome, versione, provenienza, impronta,
licenza, se serve al programma o solo alle prove. Una prova controlla che coincida con quello che si scarica davvero.
Serve a chi comprera' K-Zero e al regolamento europeo sulla cibersicurezza dei prodotti (Cyber Resilience Act), se
K-Zero sara' venduto.

## 4. I rischi che restano, e chi li chiudera'

- **File in chiaro sul disco; un solo utente; nessun registro degli accessi.** Chiunque apra la cartella legge i dati.
  Li chiude il mandato **K-010 "cassaforte dei dati personali"**: dati personali cifrati con una password, piu' utenti,
  registro degli accessi. Fino ad allora: sezione 5.
- **Nomi liberi.** Nomi di servizi, gruppi, corse, turni, calendari, note e codici sono dati comuni scelti da chi scrive
  i dati: il programma li mostra sempre. Un nome come "Trasporto al centro diurno" accanto all'elenco dei passeggeri dice
  lo stesso di un luogo sensibile. E' responsabilita' di chi scrive i dati (sezione 5). Il pacchetto dimostrativo, che ha
  dati inventati ed e' stato scritto prima di questa regola, ha proprio nomi e codici cosi' (il servizio `S-DIS`
  "Trasporto al centro diurno Il Faro", il gruppo "Centro diurno, andata", il turno `T-DIS` "Centro diurno", il
  calendario `CAL-CENTRO` "Centro diurno: lunedi', mercoledi' e giovedi'", la corsa "Centro diurno, giro principale"):
  restano come esempio di cosa evitare, e perche' le prove li usano. Nella giornata con i dati personali il centro
  diventa "riservato", ma questi nomi dicono ancora "centro diurno" (non quale).
- **Collegamenti tra i codici.** Dentro una giornata, e dentro servizi e corse della dimostrazione, il legame tra una
  casa o un passeggero e un luogo sensibile e' chiuso anche attraverso i codici (stesso mezzo, trasbordi, servizi).
  Restano: tra messaggi diversi di `kzero controlla`; tra uscite diverse (chi puo' usare `--includi-dati-personali` puo'
  anche chiedere la stessa giornata senza, dove il centro si vede accanto al codice della casa: la regola protegge
  quello che una singola uscita mostra, ad esempio un foglio dato a un autista, non chi usa il programma - quello e' il
  compito degli utenti e dei permessi del mandato K-010); gli orari e i km, per chi conosce la zona. Un'uscita con
  `--includi-dati-personali` va trattata come i file del pacchetto.
- **Uscite copiate altrove.** Quello che l'utente salva, stampa o incolla (es. in un assistente AI) esce dal controllo
  del programma.
- **Memoria.** Un file vicino al massimo di 256 MB, preparato apposta, puo' occupare in memoria diversi GB mentre lo si
  legge. Il limite ferma gli errori e i file sbagliati, non un attacco fatto per esaurire la memoria di un PC piccolo.
- **Chi ha gia' accesso al PC** puo' cambiare i file mentre il programma li legge, o usare un collegamento fisso ("hard
  link", che il sistema non distingue da un file normale): sono fuori da cio' che il programma puo' controllare.
- **Windows.** Le prove della lettura blindata (pipe, dispositivi, collegamenti e scorciatoie, cartelle sincronizzate
  come OneDrive con i "file su richiesta") e le protezioni del compilatore equivalenti (`/GS`, `/guard:cf`,
  `/DYNAMICBASE`, `/HIGHENTROPYVA`, `/NXCOMPAT`) le fara' il mandato del programma per Windows. Oggi sono provate solo
  su Linux.
- **Le librerie del sistema** (libreria C e C++ del compilatore) non sono nell'elenco dei componenti: le aggiorna il
  sistema operativo.

## 5. Consigli per chi usa K-Zero

- **Un utente per persona** sul PC, con la sua **password**, e il blocco dello schermo quando ci si allontana. I file
  dei pacchetti in una cartella dell'utente, non in una cartella condivisa.
- **Cifratura del disco** del sistema operativo: su Windows BitLocker (Pro) o "Crittografia dispositivo" (Home); su Mac
  FileVault; su Linux la cifratura del disco all'installazione. Cosi' un PC rubato non lascia leggere i file.
- **Copie di sicurezza** regolari, su un supporto separato e cifrato, e ogni tanto la prova di rimetterle a posto.
- **Livello "solo numeri" quando basta**: per i turni, i mezzi, i giri bastano i conteggi per fermata. I nomi e le case
  solo quando servono davvero.
- **Nomi liberi senza dati sulla salute.** Da evitare: "Trasporto al centro diurno", "Disabili Pradello", "Corsa
  carrozzina Rossi", "Turno terapie", una nota "Mario deve andare in dialisi", un codice `S-DIS`. Meglio: "Servizio Valfonda
  3", "Andata Il Faro" (se il luogo non e' sensibile), "Turno 4"; il tipo "disabili" va nel campo apposta, che e' gia'
  riservato.
- **Luoghi sensibili segnati**: un centro diurno, di terapia o di riabilitazione con `"sensibile": true` (luoghi.json,
  versione 2). Un doposcuola di solito no.
- **Non mandare pacchetti "completi"** per mail o su chiavette; se serve, un pacchetto "solo numeri", oppure un archivio
  cifrato con una password detta a voce.
- **Assistenti AI**: dare solo le uscite di base (senza `--includi-dati-personali` e `--includi-dati-particolari`), mai i
  file di un pacchetto "completo".
- **Programma**: scaricarlo solo dalla fonte ufficiale e tenerlo aggiornato.

## 6. Cosa fare in caso di violazione

Una violazione e' per esempio: un PC o una chiavetta con un pacchetto rubati o persi, un pacchetto o un'uscita con i nomi
mandati alla persona sbagliata, un'uscita con i dati personali incollata in un servizio esterno.

1. **Fermare il danno**: cambiare le password, chiedere a chi ha ricevuto per sbaglio di cancellare, bloccare il PC a
   distanza se si puo'.
2. **Capire cosa e' coinvolto**: `kzero controlla <cartella> --includi-dati-personali` sulla copia del pacchetto dice il
   livello ("solo numeri" o "completo") e quanti dati personali e particolari ci sono, file per file. Annotare data,
   ora, cosa e' successo, chi e' coinvolto.
3. **Avvisare**:
   - l'azienda che tratta i dati **per conto di altri** (es. i dati degli alunni per conto della Provincia) informa
     **subito il titolare**, senza ingiustificato ritardo (art. 33 c. 2): e' il titolare che decide se e come notificare
     al Garante, entro 72 ore da quando lo sa (art. 33 c. 1), e se avvisare le persone (art. 34);
   - per i dati di cui l'azienda e' titolare (es. i propri dipendenti) l'azienda valuta lei e, se serve, notifica al
     Garante entro 72 ore e avvisa le persone.
4. **Documentare** la violazione anche quando non va notificata: il titolare documenta ogni violazione (art. 33 c. 5), e
   l'azienda che tratta per conto di altri gli da' i fatti che servono.

I tempi e i modi esatti li conferma il RPD del titolare.
