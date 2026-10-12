# Codici di errore di kzero

Ogni errore ha:
- un **codice fisso** `K-ERR-nnn`: non cambia mai e non viene mai riusato per un altro errore;
- un **messaggio in italiano** che spiega il caso preciso (cosa non va e come si scrive giusto);
- la **posizione** (file, riga, colonna) quando il problema sta in un file di dati.

Il **codice di uscita** del programma dice di che tipo e' l'errore:

| Uscita | Significato |
|---|---|
| 0 | riuscito |
| 1 | errore nei dati |
| 2 | errore nei parametri |
| 3 | errore interno |

Dove va l'errore:
- il risultato va sull'uscita standard; l'errore va sull'**uscita di errore**, e il risultato resta vuoto;
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
      "messaggio": "Comando sconosciuto: 'pianifica'. Uso: kzero <comando> [parametri]. Comandi disponibili: descrivi, dimostrazione, versione. Elenco completo di comandi e parametri: kzero descrivi."
    }
  ]
}
```

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
