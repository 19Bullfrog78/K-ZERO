# K-Zero

Software di pianificazione flotta: servizi di linea, handicap, scolastici, ibridi.
Si parte da Alpinabus (3 minibus + 1 auto) e si deve poter crescere fino a 500 mezzi.

## Confini
- IMPORTANTE: questo repository riguarda SOLO K-Zero. Sullo stesso account GitHub esiste un
  progetto musicale separato: non leggerlo, non modificarlo, non citarlo, non mescolare file.

## Ruoli
- Mauro: supervisore. Decide le regole del mestiere e approva i risultati.
- Claude Code: programmatore, unico che scrive nel repository GitHub.
- Claude in chat (progetto K-Zero): arbitro. Rivede piani e modifiche, non scrive codice nel repository.

## Comandi
- Compilare: `cmake -S . -B build && cmake --build build`
- Eseguire la prova: `./build/kzero`

## Regole di lavoro
- IMPORTANTE: Mauro non e' un tecnico. Spiegazioni in italiano semplice, brevi, senza gergo.
- Prima di dire "fatto": compilare senza warning, eseguire, mostrare l'output.
- Ogni modifica va annotata in DIARIO.md. Le scelte di fondo vanno in DECISIONI.md.
- Il codice aggiornato va salvato nel progetto in CODICE/ (finche' non c'e' un archivio con storico).
- Le regole del mestiere sono in REGOLE.md: leggerle prima di toccare il calcolo.

## Scelte tecniche
- C++17, CMake, Google OR-Tools (non ancora collegato).
- Orari in minuti dalla mezzanotte, distanze in km.
- Fonte delle distanze intercambiabile: Google Maps per Alpinabus, OSRM per la scala provinciale.
- Un solo modello dati per 4 o 500 mezzi; cambia solo il metodo di calcolo.

## Trappole
- "Ibrido" non si inserisce a mano: lo ricava derive_service.
- L'assistente occupa un posto; ogni carrozzina toglie posti a sedere.
- Pause a casa dell'autista e ricoveri esterni sono preferenze, mai obblighi.
- I valori in src/main.cpp sono inventati: non usarli come dati reali.
