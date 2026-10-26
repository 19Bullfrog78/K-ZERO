# Codici di errore di kzero

Questi sono gli errori **del programma** (riga di comando sbagliata, cartella inesistente, guasti).
I problemi **nei dati** di un pacchetto hanno un catalogo a parte, con codici `K-CTR-nnn`: vedi
[controlli.md](controlli.md).

Ogni errore ha:
- un **codice fisso** `K-ERR-nnn`: non cambia mai e non viene mai riusato per un altro errore;
- un **messaggio in italiano** che spiega il caso preciso (cosa non va e come si scrive giusto);
- la **posizione** (file, riga, colonna) quando esiste.

Il **codice di uscita** del programma dice di che tipo e' l'errore:

| Uscita | Significato |
|---|---|
| 0 | riuscito |
| 1 | errore nei dati |
| 2 | errore nei parametri |
| 3 | errore interno |

Dove va l'errore:
- il risultato va sull'uscita standard; l'errore va sull'**uscita di errore**, e il risultato resta vuoto
  (`kzero controlla` su un pacchetto con errori nei dati non e' un errore del programma: il resoconto va
  sull'uscita standard e il codice di uscita e' 1);
- con `--formato json` anche l'errore e' un documento JSON (schema `errore`,
  file [schema/errore.v1.schema.json](schema/errore.v1.schema.json)); se `--formato json` e' scritto
  ma la riga di comando e' sbagliata, l'errore e' comunque in JSON.

Esempio in testo:

```
$ kzero versione --colore
Errore K-ERR-003: Parametro sconosciuto per il comando 'versione': '--colore'. Parametri ammessi: --formato. Elenco completo di comandi e parametri: kzero descrivi.
```

Esempio in JSON (`kzero pianifica --formato json`, codice di uscita 2):

```json
{
  "schema": "errore",
  "schema_versione": 1,
  "codice_uscita": 2,
  "errori": [
    {
      "codice": "K-ERR-002",
      "messaggio": "Comando sconosciuto: 'pianifica'. Uso: kzero <comando> [argomenti] [parametri]. Comandi disponibili: controlla, descrivi, dimostrazione, versione. Elenco completo di comandi e parametri: kzero descrivi."
    }
  ]
}
```

Gli errori del programma non sanno quali parametri sono stati scritti: la loro posizione non nomina mai un file di
dati che, tutto intero, e' un dato personale (es. `assenze.json`, vedi controlli.md). Oggi nessun errore K-ERR ha una
posizione in un file di dati.

Quando l'errore ha una posizione, nel testo compare tra parentesi
(`Errore K-ERR-nnn (mezzi.csv, riga 3, colonna 5): ...`) e nel JSON nel campo `posizione`
(`"posizione": {"file": "mezzi.csv", "riga": 3, "colonna": 5}`), con le sole parti note.

## Elenco completo

Questo elenco deve coincidere con `kzero descrivi`: una prova automatica
(`documentazione.errori_e_schemi`) controlla le prime tre colonne.
Un nuovo errore prende il codice successivo all'ultimo.

| Codice | Uscita | Descrizione | Quando succede | Cosa fare |
|---|---|---|---|---|
| K-ERR-001 | 2 | Manca il comando | Non c'e' nessun argomento, oppure il primo argomento e' un parametro (es. `kzero --formato json versione`). | Scrivere il comando subito dopo `kzero`: `kzero <comando> [parametri]`. |
| K-ERR-002 | 2 | Comando sconosciuto | Il primo argomento non e' un comando di kzero. | Usare uno dei comandi elencati nel messaggio o da `kzero descrivi`. |
| K-ERR-003 | 2 | Parametro sconosciuto | Un parametro che il comando non conosce (es. `--colore`, o `--includi-dati-personali` con `versione`). | Usare solo i parametri elencati nel messaggio. |
| K-ERR-004 | 2 | Argomento non previsto | Un valore senza parametro (es. `kzero versione extra`), due comandi insieme, oppure `--`. | Togliere l'argomento in piu'; dare un solo comando alla volta. |
| K-ERR-005 | 2 | Manca il valore del parametro | Un parametro che vuole un valore e' scritto senza (es. `--formato` alla fine, o `--formato=`). | Aggiungere uno dei valori ammessi elencati nel messaggio. |
| K-ERR-006 | 2 | Valore non ammesso | Il valore non e' fra quelli ammessi (es. `--formato xml`). | Usare uno dei valori ammessi elencati nel messaggio. |
| K-ERR-007 | 2 | Parametro ripetuto | Lo stesso parametro e' scritto due volte. | Scriverlo una volta sola. |
| K-ERR-008 | 2 | Il parametro non accetta valori | Un interruttore e' scritto con un valore (es. `--includi-dati-personali=no`). | Scrivere l'interruttore da solo, oppure toglierlo. |
| K-ERR-009 | 3 | Errore interno | Un guasto del programma, non un errore di chi lo usa. | Segnalare il problema con la riga di comando usata. |
| K-ERR-010 | 3 | Impossibile scrivere il risultato | Il risultato non si riesce a scrivere (es. disco pieno, uscita chiusa). | Controllare dove si sta scrivendo il risultato e riprovare. |
| K-ERR-011 | 2 | Manca un argomento | Un comando che vuole un argomento e' scritto senza (es. `kzero controlla` senza la cartella). | Scrivere l'argomento come indicato nel messaggio (es. `kzero controlla <cartella>`). |
| K-ERR-012 | 1 | Cartella del pacchetto non trovata | La cartella indicata a `kzero controlla` o `kzero giornata` non esiste o non e' una cartella. | Indicare la cartella che contiene il manifesto `pacchetto.json`. |
| K-ERR-013 | 2 | Manca un parametro obbligatorio | Un comando che vuole sempre un parametro e' scritto senza (es. `kzero giornata esempi/demo` senza `--data`). | Aggiungere il parametro come indicato nel messaggio (es. `--data 2026-10-15`). |
| K-ERR-014 | 2 | Valore scritto nella forma sbagliata | Il valore di un parametro non ha la forma richiesta: una data che non e' AAAA-MM-GG o che non esiste (es. `--data 2026-02-30`), un codice con caratteri non ammessi (es. `--persona "P A"`). | Scrivere il valore nella forma indicata nel messaggio. |
| K-ERR-015 | 2 | Parametri che non si possono usare insieme | Due parametri che si escludono sono scritti insieme (es. `--persona` e `--mezzo` in `kzero giornata`). | Tenere uno dei due. |
| K-ERR-016 | 2 | Data fuori dal periodo del pacchetto | La data chiesta (es. a `kzero giornata`) non sta nel periodo del manifesto del pacchetto: il pacchetto non dice nulla di quel giorno. | Scegliere una data del periodo, scritto nel messaggio, oppure usare il pacchetto del periodo giusto. |
| K-ERR-017 | 2 | Persona o mezzo non presente nel pacchetto | Il codice dato a `--persona` o a `--mezzo` non c'e' in `persone.json` o in `mezzi.json`. | Controllare il codice (le maiuscole contano). |
| K-ERR-018 | 1 | Il pacchetto ha errori nei dati | Un comando che mostra un risultato calcolato dai dati (es. `kzero giornata`) trova errori nel pacchetto: un risultato su dati sbagliati sarebbe falsato, ed e' peggio di nessun risultato. Gli avvisi non bloccano. | Correggere gli errori elencati da `kzero controlla <cartella>`, poi riprovare. |
| K-ERR-019 | 2 | Password non disponibile (nessun terminale e niente --password-da-ingresso) | Un comando sulla cassaforte deve chiedere una password, ma il programma non ha un terminale (es. lanciato da un altro programma) e non c'e' `--password-da-ingresso`; oppure l'ingresso standard finisce prima delle password attese. La password non si scrive mai come parametro ne' in una variabile d'ambiente. | Lanciare il comando da un terminale, oppure passare le password sull'ingresso standard, una per riga, con `--password-da-ingresso`. |
| K-ERR-020 | 2 | Password nuova non ammessa | Una password nuova e' troppo corta (meno di 15 caratteri), troppo lunga, comune, prevedibile (es. fatta del codice dell'utente) o con caratteri non ammessi; oppure, dal terminale, le due volte non coincidono. Il messaggio dice quale regola (docs/sicurezza.md). | Scegliere un'altra password: una frase lunga e' la scelta migliore. |
| K-ERR-021 | 2 | Utente sconosciuto nella cassaforte | Il codice dato a `--utente` (o a `--utente-da-togliere`, `--per-utente`) non e' tra gli utenti della cassaforte. | Controllare il codice (le maiuscole contano); `kzero cassaforte` elenca gli utenti. |
| K-ERR-022 | 2 | Password sbagliata | La password non apre la fessura dell'utente (oppure la fessura e' stata cambiata da fuori di K-Zero: le due cose non si possono distinguere). Nessun dato si apre. Non c'e' recupero: un altro utente con gli stessi permessi puo' dare una password nuova (`kzero password --per-utente`). | Riprovare con la password giusta. |
| K-ERR-023 | 2 | Permesso mancante | Si chiede qualcosa oltre i permessi dell'utente: `--includi-dati-personali` o `--includi-dati-particolari` senza quel permesso; dare a un altro utente permessi che non si hanno; togliere o cambiare la password di un utente con permessi che non si hanno; richiudere, cambiare la conservazione o cancellare senza i permessi di tutte e due le classi. | Chiedere a un utente con quei permessi. |
| K-ERR-024 | 1 | Cassaforte vuota, troncata o rovinata | Il file `cassaforte.kzc` non e' una cassaforte di K-Zero, oppure e' vuoto, troncato o rovinato (es. copia interrotta). Nessun dato si apre. | Rimettere una copia buona del pacchetto. |
| K-ERR-025 | 1 | Cassaforte di una versione del formato sconosciuta | La cassaforte e' stata scritta da un programma K-Zero piu' recente, con una versione del formato che questo programma non conosce. | Usare un programma K-Zero aggiornato. |
| K-ERR-026 | 1 | Cassaforte di un altro pacchetto | Il codice della cassaforte non e' quello scritto nel manifesto: la cassaforte viene da un'altra cartella. | Rimettere la cassaforte di questo pacchetto. |
| K-ERR-027 | 1 | Parametri della password assurdi o sotto il minimo | La fessura dell'utente chiede parametri di Argon2id fuori da quelli che il programma accetta: troppo bassi (abbassati da fuori, per rendere la password piu' facile da indovinare) o assurdi (es. memoria enorme, per bloccare il PC). Rifiutati prima di calcolare. | Rimettere una copia buona del pacchetto. |
| K-ERR-028 | 1 | Cassaforte cambiata da fuori di K-Zero | L'intestazione, una busta, la storia delle chiavi, gli elenchi dei permessi o le sezioni dei dati non sono autentici: qualcuno ha cambiato il file con un altro programma (es. un utente aggiunto da chi non ha una password, una chiave pubblica sostituita, un permesso alzato a mano). Nessun dato si apre e nessuna chiave si consegna. | Rimettere una copia buona del pacchetto e capire chi ha cambiato il file. |
| K-ERR-029 | 1 | Registro degli accessi mancante, rovinato o cambiato | Il file `registro.kzr` manca, e' rovinato, e' di un'altra cassaforte, ha la catena delle voci rotta o meno voci di quelle che la cassaforte ricorda. I dati non si aprono: ogni apertura deve essere scritta nel registro. | Rimettere una copia buona del pacchetto (cassaforte e registro insieme). |
| K-ERR-030 | 1 | Scrittura sul disco non riuscita (registro, cassaforte o file) | Un file non si riesce a scrivere o la cartella non si riesce a bloccare (disco pieno, permessi, disco di rete che non permette il blocco). Se e' la voce del registro, i dati non si mostrano. Il file di prima resta intero. | Controllare spazio e permessi della cartella e riprovare. |
| K-ERR-031 | 2 | Il pacchetto non ha una cassaforte | Un comando della cassaforte (es. `kzero registro`), o `--utente`, su un pacchetto senza cassaforte. | Chiudere prima il pacchetto (`kzero chiudi`), oppure togliere `--utente`. |
| K-ERR-032 | 2 | Il pacchetto ha una cassaforte: serve --utente | Il pacchetto ha i dati personali nella cassaforte: per aprirla serve sapere chi la apre. | Aggiungere `--utente <codice>`; la password si chiede dopo. |
| K-ERR-033 | 2 | La cartella da scrivere c'e' gia' | `kzero chiudi` e `kzero esporta` scrivono sempre in una cartella nuova, per non mescolare mai dati in chiaro e dati chiusi. | Indicare con `--scrivi` una cartella che non c'e' ancora. |
| K-ERR-034 | 1 | Manifesto da aggiornare alla versione 2 prima di chiudere | Il manifesto del pacchetto da chiudere e' della versione 1, che non ha la dichiarazione della cassaforte ne' la scelta `porta_a_porta_sensibile`. La scelta e' dell'utente: il programma non la fa da solo. | Portare `pacchetto.json` alla versione 2 (docs/formato-dati.md) e riprovare. |
| K-ERR-035 | 1 | Dati riaperti diversi dall'originale: niente scritto | Dopo la chiusura (o la richiusura) il pacchetto riaperto non da' gli stessi dati dell'originale: di solito due elementi con lo stesso codice, che la cassaforte non saprebbe distinguere. Nella cartella non resta niente di nuovo. | Correggere i codici ripetuti (kzero controlla) e riprovare. |
| K-ERR-036 | 2 | Modifica degli utenti non ammessa | Un utente con quel codice c'e' gia'; oppure si vuole togliere l'ultimo utente, o l'ultimo che legge i dati personali o particolari (i dati si perderebbero: per togliere tutto c'e' `kzero cancella`). | Scegliere un altro codice, o aggiungere prima un altro utente con quel permesso. |
| K-ERR-037 | 1 | Pacchetto in chiaro diverso da quello chiuso | `kzero richiudi` riceve un pacchetto in chiaro con un altro codice di pacchetto, o che dichiara gia' una cassaforte: non e' l'uscita in chiaro di quel pacchetto. | Richiudere l'uscita in chiaro giusta (`kzero esporta`). |
| K-ERR-038 | 1 | Il manifesto dichiara una cassaforte che manca | Un comando della cassaforte su un pacchetto il cui manifesto dichiara la cassaforte, ma il file `cassaforte.kzc` non c'e' (es. cartella copiata a meta'). | Rimettere il file della cassaforte preso dalla stessa copia del pacchetto. |
