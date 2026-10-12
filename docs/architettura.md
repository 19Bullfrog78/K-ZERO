# Architettura di K-Zero

## In breve

K-Zero ha due utenti: **una persona** (chi pianifica) e **un'AI** (un assistente che lavora per lei).
Tutti e due usano lo stesso programma, `kzero`, con gli stessi comandi. La persona legge l'uscita
in **testo** (italiano chiaro); l'AI legge l'uscita in **JSON**, descritta da **schemi** pubblici.
Gli errori hanno sempre un **codice fisso** (`K-ERR-nnn`).

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
- Riceve dati gia' controllati e restituisce risultati. Per questo si prova da solo (GoogleTest),
  senza passare dalla riga di comando.
- Oggi: `domain_models.hpp` (modello e regole) e `dimostrazione.cpp` (scenario con dati inventati).

## Piano 2 - API (`src/api`): il contratto ufficiale

- **Oggi** l'API e' la riga di comando: `kzero <comando> [parametri]`, con uscita in testo o JSON.
- **Domani**, quando ci saranno piu' utenti, la stessa API si potra' offrire via HTTP con una descrizione
  OpenAPI: stessi comandi, stessi documenti JSON. Gli schemi draft-07 di oggi si riusano con piccoli
  adattamenti (OpenAPI 3.1 usa JSON Schema).
- Pezzi:

| File | Compito |
|---|---|
| `comandi.cpp` | **Tabella dei comandi**: nome, descrizione, parametri, schema dell'uscita. E' l'unico posto dove si dichiarano. Da qui nascono la lettura dei parametri e `kzero descrivi`. |
| `parametri.cpp` | Lettura dei parametri con CLI11; i controlli e i messaggi in italiano sono di K-Zero. |
| `errori.cpp` | Catalogo dei codici K-ERR e presentazione degli errori in testo o JSON. |
| `uscita.cpp` | Strumenti comuni: documento JSON con "schema" e "schema_versione", arrotondamenti, orari. |
| `comando_*.cpp` | Un file per comando: chiede i risultati al nucleo e li presenta in testo o JSON. |
| `esegui.cpp` | Dalla riga di comando all'esito: prepara tutto in memoria (`esegui`), poi lo scrive (`consegna`). Cosi' un errore non lascia mai un risultato a meta'. |

- I futuri **lettori dei file di dati** (K-007 formato standard, K-008 file del CTA) staranno in questo
  piano: trasformano i file nel modello del nucleo e segnalano gli errori con file, riga e colonna.

## Piano 3 - Adattatori

- **Interfaccia per persone** (K-009: programma per Windows da scaricare) ed eventuale **adattatore MCP**
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
4. Di base il JSON non contiene dati personali: solo codici e conteggi. Il formato testo invece e' pensato
   per le persone e li mostra: **un agente AI deve usare il JSON**.

## Regole del contratto

Sono elencate in CLAUDE.md ("Principi uomo + AI") e valgono per ogni comando futuro. In sintesi:
un solo eseguibile con comandi; `--formato testo|json`; schema e versione in ogni JSON; errori con codice
fisso; codici di uscita 0 riuscito, 1 dati, 2 parametri, 3 interno; stesso input = stesso output
(niente orologio ne' caso, date solo con `--data`); nessun file scritto senza `--scrivi`; dati personali
solo con `--includi-dati-personali`.

Scelte di dettaglio:
- il comando e' sempre il primo argomento; non c'e' `--help`: la descrizione completa e' `kzero descrivi`
  (un'unica fonte, in italiano). Chi scrive `--help` riceve un errore che indica `kzero descrivi`;
- con `--formato json` scritto da qualche parte, anche una riga di comando sbagliata riceve l'errore in JSON;
- se la forma di un'uscita JSON cambia, cambia `schema_versione` e si aggiunge un nuovo file di schema;
- gli elenchi escono in ordine fisso (alfabetico, o per codice);
- nel JSON i valori calcolati sono arrotondati: km al metro, euro al centesimo;
- i dati personali stanno in un unico blocco `dati_personali`, collegato al resto dai codici.
  Nel servizio porta a porta la fermata e' la casa del passeggero: per questo anche i luoghi delle
  fermate sono dati personali.

## Come si aggiunge un comando

1. Una voce nella tabella di `src/api/comandi.cpp` (nome, descrizione, parametri, schema).
2. Un file `src/api/comando_<nome>.cpp` che chiede i risultati al nucleo e prepara testo e JSON.
3. Lo schema `docs/schema/<schema>.v1.schema.json`.
4. Eventuali nuovi errori nel catalogo (`src/api/errori.cpp`) e in docs/errori.md.
5. Le prove in `tests/CMakeLists.txt` (`kzero_prova(...)`).

`kzero descrivi` si aggiorna da solo; le prove controllano che docs/errori.md e docs/schema coincidano.

## Prove

| Etichetta | Cosa controlla |
|---|---|
| `nucleo` | Regole del mestiere e scenario di dimostrazione (GoogleTest). |
| `api` | Tabella dei comandi, catalogo degli errori, errori con posizione, guasto in scrittura (GoogleTest). |
| `programma` | Lanciano `kzero` due volte: codice di uscita, K-ERR con uscita 2 per i parametri sbagliati, JSON valido, uscite identiche byte per byte, nessun dato personale nel JSON di base, stampa di prova uguale a quella di sempre. |
| `documentazione` | docs/errori.md e docs/schema coincidono con `kzero descrivi`. |
| `architettura` | Il nucleo non stampa e non legge la riga di comando. |
| `schemi` | Con lo strumento standard check-jsonschema: schemi validi per draft-07; ogni uscita JSON delle prove rispetta lo schema che dichiara. |

## Limiti noti

- Il formato testo mostra sempre i dati personali (e' per le persone): la protezione vale per il JSON.
- Nessun comando di oggi legge dati: il codice di uscita 1 (errore nei dati) e le posizioni negli errori
  saranno usati da K-007 e K-008 (il formato e' gia' pronto e provato).
- Windows (K-009): argomenti con lettere accentate e a capo "CR LF" sono da sistemare e provare li'.
- Stesso input = stesso output vale a parita' di programma e di sistema. Tra sistemi diversi i valori
  calcolati con decimali potrebbero differire all'ultima cifra: per questo il JSON li arrotonda.
