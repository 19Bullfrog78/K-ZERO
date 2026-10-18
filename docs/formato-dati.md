# Formato dei dati di K-Zero (parte 1: rete e servizi)

Questo documento spiega come si scrivono i dati che K-Zero legge: luoghi e fermate, regolamenti,
calendari, scuole e centri, committenti e contratti, servizi con la loro domanda, corse, mezzi e rimesse.
La seconda parte del formato (autisti, turni, rotazioni, blocchi) arrivera' con il mandato K-008.

Il formato vale per qualunque azienda, consorzio o ente, di qualunque dimensione e di qualunque paese:
nel programma non c'e' nulla di specifico di un'azienda o di un territorio. Le regole di un territorio
stanno in un **regolamento**, che e' un dato; le parole di un'azienda (es. "primo", "lotto 2") sono solo
nomi nei dati.

Per controllare un pacchetto: `kzero controlla <cartella>` (problemi spiegati in [controlli.md](controlli.md)).
Esempi completi: `esempi/demo` (livello "completo") ed `esempi/demo-numeri` (livello "solo numeri"),
con dati inventati.

## Il pacchetto dati

Un **pacchetto** e' una cartella con un **manifesto** (`pacchetto.json`) e un file JSON per ogni tipo di dato.
Solo il manifesto e' obbligatorio; un file che manca vuol dire "nessun elemento di quel tipo".

| File | Obbligatorio | Contenuto |
|---|---|---|
| `pacchetto.json` | si' | Manifesto: aziende, periodo, origine, livello, fuso orario, valuta, impostazioni |
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
| `mezzi.json` | no | Mezzi |
| `rimesse.json` | no | Rimesse e ricoveri dei mezzi |
| `tempi_sosta.json` | no | Tempi di sosta alle fermate (facoltativi finche' non confermati) |

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
  (es. sistema "Trentino Trasporti", tipo "fermata", codice "6220PF"; sistema "CTA", tipo "servizio",
  codice "1050002/3"). Cosi' il formato vale per qualunque committente senza campi dedicati.
- **Orari** `"HH:MM"`, ora locale del fuso del pacchetto. Dopo mezzanotte si continua a contare, come in GTFS:
  una corsa che parte alle 23:40 e arriva alle 00:12 del giorno dopo si scrive `"23:40"` ... `"24:12"`.
  Ammessi fino a `"47:59"`. Anche nelle uscite del programma un orario oltre le 24:00 resta tale.
- **Date** `"AAAA-MM-GG"`, solo date che esistono (il 29 febbraio solo negli anni bisestili).
- **Coordinate** WGS84 in gradi decimali: `{"lat": 46.4421, "lon": 11.0712}`.
- **Distanze** in km (nei regolamenti, dove il nome del campo lo dice, in metri: `_m`); **misure** in metri,
  **masse** in tonnellate; **costi** nella valuta del manifesto.
- **Testi**: una riga non vuota fino a 300 caratteri; le note (`nota`) fino a 2000 caratteri.
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
e le corse notturne di quei due giorni vanno controllate a mano.

### Classi dei dati

Ogni campo e' marcato nello schema con la sua **classe** (`"x-kzero-classe"`). Il filtro delle uscite
parte da quella marcatura ed e' uguale per testo e JSON.

| Classe | Cosa comprende | Nelle uscite, di base | Per vederli |
|---|---|---|---|
| comune | fermate pubbliche (nome, posizione, codici), scuole e centri, grado, orari, calendari, conteggi senza esigenze, mezzi (targa compresa), aziende, committenti, contratti, regolamenti, tutti i codici | sempre visibili | - |
| personale | nomi e cognomi, indirizzi, case, punti porta a porta (sono la casa del passeggero), ricoveri presso la casa di una persona, motivi scritti a mano | sostituiti dal **codice** dell'elemento | `--includi-dati-personali` |
| particolare | dati sulla salute: esigenze di una persona o di un gruppo (carrozzina, ausili piegati, assistente necessario), tipo di servizio e di gruppo ("disabili"), conteggi di carrozzine e di ausili piegati, pedana obbligatoria per un servizio, categoria ricavata della corsa | sostituiti da `"riservato"` | `--includi-dati-particolari` |

Perche' i dati particolari sono nascosti anche quando al posto del nome c'e' un codice: sono dati sulla
salute, e restano dati personali anche senza nome (GDPR, considerando 26); in un paese piccolo
"1 carrozzina alla fermata X" basta a riconoscere una persona. L'uscita grezza del programma spesso la
legge un assistente AI su server esterni: per questo di base non li mostra. L'interfaccia per persone
(futura) li mostrera' a chi pianifica.

Un campo senza marcatura vale, per prudenza, come particolare: una prova controlla che tutti i campi
abbiano la loro classe, che i dati personali stiano solo nei luoghi privati e nei passeggeri, e quali sono
i dati particolari.

### Due livelli di pacchetto

Il manifesto dichiara il **livello**:
- **`solo_numeri`** (consigliato): conteggi per fermata; nessun nome, indirizzo, punto porta a porta o
  motivo scritto. Niente luoghi privati e niente passeggeri uno per uno (errore K-CTR-019);
- **`completo`**: anche i passeggeri uno per uno e le loro case.

I conteggi di carrozzine e di ausili piegati (dati particolari aggregati) sono ammessi in entrambi i livelli,
sempre filtrati nelle uscite.

**Minimizzazione:** il formato non ha campi per codice fiscale, telefoni, mail delle famiglie, data di
nascita. K-Zero non li conserva mai: per pianificare non servono. Un campo cosi' aggiunto a mano e' un
errore ("campo non previsto").

### Come si legge la tabella dei campi

Per ogni file c'e' una tabella: **campo** (con il percorso: `luoghi[].nome` = il nome di ogni luogo
dell'elenco `luoghi`), **obbligatorio** (si'/no), **classe** (comune, personale, particolare) e
**significato**. I campi `schema` e `schema_versione`, presenti in ogni file, non sono ripetuti.

## I file, uno per uno

### pacchetto.json

Il manifesto: chi, quando, da dove vengono i dati, che livello di dati contiene. Schema:
[docs/schema/pacchetto.v1.schema.json](schema/pacchetto.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `codice` | si' | comune | Codice del pacchetto (serve a dire da quale pacchetto ne deriva un altro) |
| `nome` | no | comune | Nome del pacchetto |
| `dati_inventati` | no | comune | `true` = dati inventati (esempi, prove): non usarli come dati reali |
| `aziende[]` | si' | comune | Aziende presenti nel pacchetto (una o piu'): `codice`, `nome` |
| `periodo` | si' | comune | Periodo di validita' dei dati: `dal`, `al` (compresi) |
| `origine.tipo` | si' | comune | `rilevato_a_mano`, `calcolato`, `importato` oppure `scenario` |
| `origine.deriva_da` | no | comune | Codice del pacchetto da cui deriva |
| `origine.nota` | no | comune | Note sull'origine dei dati |
| `livello` | si' | comune | `solo_numeri` (consigliato) oppure `completo` |
| `fuso_orario` | si' | comune | Nome IANA del fuso (es. `Europe/Rome`): gli orari sono orari locali di questo fuso |
| `valuta` | si' | comune | Codice ISO 4217 (es. `EUR`): i costi sono in questa valuta |
| `impostazioni.margine_trasbordo_minuti` | si' | comune | Scelta di chi pianifica: la corsa che riceve un trasbordo deve arrivare almeno tanti minuti prima della corsa che consegna |

```json
{
  "schema": "pacchetto",
  "schema_versione": 1,
  "codice": "DEMO-2026",
  "nome": "Pacchetto dimostrativo K-Zero",
  "dati_inventati": true,
  "aziende": [{ "codice": "ALFA", "nome": "Autoservizi Alfa" }, { "codice": "BETA", "nome": "Trasporti Beta" }],
  "periodo": { "dal": "2026-09-01", "al": "2027-08-31" },
  "origine": { "tipo": "scenario", "nota": "Dati inventati per la dimostrazione e le prove." },
  "livello": "completo",
  "fuso_orario": "Europe/Rome",
  "valuta": "EUR",
  "impostazioni": { "margine_trasbordo_minuti": 3 }
}
```

### luoghi.json

Tutti i luoghi: `luoghi` (pubblici) e `luoghi_privati` (case di persone, dati personali). Schema:
[docs/schema/luoghi.v1.schema.json](schema/luoghi.v1.schema.json).

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
| `luoghi[].trasbordo` | no | comune | Presente = punto di trasbordo autorizzato: `autorizzato_da` (es. "CTA sentita la Provincia"), `data`, `documento` oppure `a_voce: true` (avviso K-CTR-043), `nota` |
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
regolamento che vale per i suoi servizi. Schema: [docs/schema/regolamenti.v1.schema.json](schema/regolamenti.v1.schema.json).

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

Schema: [docs/schema/committenti.v1.schema.json](schema/committenti.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `committenti[].codice` | si' | comune | Codice del committente |
| `committenti[].nome` | si' | comune | Nome (es. un consorzio, una Provincia, un Comune, un'associazione) |
| `committenti[].codici_esterni[]` | no | comune | Codici in altri sistemi |
| `committenti[].nota` | no | comune | Note |

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
[docs/schema/servizi.v1.schema.json](schema/servizi.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `servizi[].codice` | si' | comune | Codice del servizio |
| `servizi[].nome` | no | comune | Nome |
| `servizi[].codici_esterni[]` | no | comune | Es. sistema "CTA", tipo "servizio", codice "1050002/3"; scheda handicap "38177" |
| `servizi[].tipo` | si' | particolare | Tipo dichiarato dal committente: `linea`, `scolastico`, `disabili`, `altro` (con `tipo_descrizione`). "Ibrido" non si dichiara: lo ricava il programma sulla corsa |
| `servizi[].tipo_descrizione` | con `altro` | particolare | Descrizione del tipo |
| `servizi[].contratto` | si' | comune | Contratto |
| `servizi[].calendario` | si' | comune | Calendario |
| `servizi[].liberta_oraria.tipo` | si' | comune | `fissa` (orari del committente, es. l'orario pubblico di una linea) oppure `libera` (liberi dentro il regolamento) |
| `servizi[].liberta_oraria.soglia_da_concordare_minuti` | con `libera` | comune | Scelta dall'utente: oltre questo spostamento un cambio d'orario va segnalato come "da concordare" |
| `servizi[].regole` | no | comune | Regole proprie del servizio, se diverse dal contratto: `posti_minimi`, `posti_massimi`, `durata_massima_a_bordo_minuti`, `categoria_riconosciuta` |
| `servizi[].regole.pedana_obbligatoria` | no | particolare | Pedana obbligatoria per questo servizio: dice che a bordo viaggia una carrozzina |
| `servizi[].domanda.gruppi[]` | si' | | Gruppi di passeggeri (sotto) |
| `servizi[].nota` | no | comune | Note |

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
allora e' una corsa ibrida (es. medie del consorzio + superiori a supporto di una linea). Schema:
[docs/schema/corse.v1.schema.json](schema/corse.v1.schema.json).

| Campo | Obbl. | Classe | Significato |
|---|---|---|---|
| `corse[].codice` | si' | comune | Codice della corsa |
| `corse[].nome` | no | comune | Nome |
| `corse[].codici_esterni[]` | no | comune | Es. sistema "Trentino Trasporti": linea "B634", corsa "533"; turno "2666" |
| `corse[].servizi[]` | si' | comune | Servizi che la corsa soddisfa (piu' di uno = ibrida; nessuno: avviso K-CTR-040) |
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

Schema: [docs/schema/mezzi.v1.schema.json](schema/mezzi.v1.schema.json). I posti non cambiano con l'eta'
dei passeggeri.

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
| `mezzi[].nota` | no | comune | Note |

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

GTFS e' il formato aperto degli orari del trasporto pubblico (lo pubblicano Trentino Trasporti e molti altri).
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
| mezzi, rimesse, tratti con i limiti, tempi di sosta | nessuno | |

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
- **Corsa ibrida** - corsa che soddisfa piu' servizi, o che porta gruppi di tipi diversi, o studenti verso
  un centro. Non si dichiara: la ricava il programma.
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
