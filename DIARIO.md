# Diario K-Zero

## Progressi
- 2026-10-11 - Creata l'ossatura: modello dati (passeggeri, destinazioni, mezzi, autisti, assistenti,
  ricoveri, contratti, corse, turni) e programma di prova. Compila senza errori ne' warning.
- 2026-10-11 - Aperto l'archivio con storico su GitHub (repository privato 19Bullfrog78/K-ZERO):
  primo salvataggio dell'ossatura su main. Compila senza errori ne' warning.
- 2026-10-11 - Tolta da CLAUDE.md la regola sulla cartella CODICE/ (l'archivio ufficiale e' solo GitHub)
  e tolto dai prossimi passi il punto "archivio con storico", perche' fatto.

## Bug aperti
- (nessuno)

## Idee
- Generare scenari "gonfiati" (50-500 mezzi) dai giri reali per provare il carico.
- Chiedere i giri ad altre aziende del consorzio come secondo banco di prova.
- Indicatori: % km a vuoto e minuti fermi, per turno e per flotta.
- Spostare nel contratto altre regole del committente (mezzo minimo, pedana, tempi massimi).

## Prossimi passi
1. Test automatici sui casi gia' noti (es. 38 ragazzi su 16 posti = 3 giri).
2. Raccolta dati veri Alpinabus: mezzi, autisti (paese), giri di una giornata tipo.
3. "Fotografia" dei giri attuali: km pieni, km a vuoto, tempi morti.
4. Lettura dei paper e proposta del metodo di calcolo con OR-Tools.
