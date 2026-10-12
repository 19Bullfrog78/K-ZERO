# Decisioni di fondo

Formato: data - decisione - perche'.

- 2026-10-11 - Si parte dall'azienda Alpinabus come banco di prova - dati noti, risultati controllabili a occhio.
- 2026-10-11 - Si calcola una giornata tipo, non l'anno intero - l'anno e' la giornata tipo piu' le variazioni.
- 2026-10-11 - Studenti raggruppati in fermate - meno punti da calcolare e meno dati personali di minori.
- 2026-10-11 - "Ibrido" calcolato in automatico dai passeggeri e dalle destinazioni - evita errori di inserimento.
- 2026-10-11 - Chi fornisce l'assistente si decide nel contratto, non sulla persona - la stessa assistente puo' essere pagata da enti diversi.
- 2026-10-11 - Costo fisso giornaliero oltre al costo al km - senza, il confronto 16 vs 30 posti sarebbe falso.
- 2026-10-11 - Fonte delle distanze intercambiabile - Google Maps gratis per Alpinabus, troppo caro per la provincia (OSRM).
- 2026-10-11 - Per pochi mezzi un unico calcolo; divisione in due fasi (corse poi concatenazione) solo quando si cresce.
- 2026-10-11 - Prove automatiche con GoogleTest 1.15.2, scaricato da CMake con versione e impronta fissate - standard del settore; ogni prova si registra da sola senza elenchi da tenere allineati; la compilazione resta uguale nel tempo.
- 2026-10-11 - K-006 - Architettura in tre piani: nucleo (modello, regole, calcolo; niente stampa ne' riga di comando), API documentata come contratto ufficiale (oggi riga di comando con JSON e schemi, domani HTTP con OpenAPI), adattatori sopra l'API senza logica (interfaccia per persone, eventuale MCP per le AI) - due utenti, persona e AI, con un solo contratto; il calcolo si prova da solo; una nuova interfaccia non tocca il calcolo. Dettagli in docs/architettura.md.
- 2026-10-11 - K-006 - Un solo eseguibile kzero con comandi; il comando e' sempre il primo argomento; niente --help, la descrizione completa e' kzero descrivi, generata dalla tabella dei comandi - un solo modo di chiamare il programma e una sola descrizione, in italiano, che non puo' restare indietro rispetto al codice.
- 2026-10-11 - K-006 - JSON con "schema" e "schema_versione"; schemi JSON Schema draft-07 in docs/schema, uno per versione; ogni cambio della forma = nuova versione - chi legge sa sempre cosa aspettarsi; draft-07 e' letto sia dagli strumenti standard sia dalle librerie C++ che controlleranno i dati in ingresso.
- 2026-10-11 - K-006 - Errori con codice fisso K-ERR-nnn numerato in ordine (mai riusato), messaggio italiano, posizione quando esiste; risultato sull'uscita standard, errori sull'uscita di errore; codici di uscita 0/1/2/3 - una persona legge il messaggio, un programma il codice; un errore non lascia mai un risultato a meta'.
- 2026-10-11 - K-006 - Nel JSON solo codici e conteggi; nomi, indirizzi e luoghi delle fermate in un blocco separato "dati_personali", solo con --includi-dati-personali - meno dati personali (anche di minori) verso programmi e assistenti AI; un solo punto da controllare. Nel porta a porta la fermata e' la casa del passeggero.
- 2026-10-11 - K-006 - CLI11 2.7.2 (un solo file, versione e impronta fissate) per leggere i parametri - libreria diffusa, senza dipendenze; i controlli e i messaggi in italiano li fa K-Zero, perche' quelli di CLI11 sono in inglese.
- 2026-10-11 - K-006 - nlohmann/json 3.12.0 (versione e impronta fissate; firma dell'autore verificata) per il JSON - la libreria JSON piu' usata in C++; su di essa si appoggiano i principali controllori draft-07 in C++, utili per K-007.
- 2026-10-11 - K-006 - Controllo degli schemi con check-jsonschema 0.38.2 (versioni e impronte di tutte le dipendenze fissate in tests/strumenti) nel controllo GitHub - strumento standard e indipendente dal nostro codice: se il programma e gli schemi non coincidono, il controllo diventa rosso.
- 2026-10-11 - K-006 - Nel JSON i valori calcolati sono arrotondati (km al metro, euro al centesimo); controllo GitHub su ubuntu-24.04 fisso invece di "ubuntu-latest" - uscite leggibili e stabili; il controllo non cambia da solo quando GitHub aggiorna il sistema.
