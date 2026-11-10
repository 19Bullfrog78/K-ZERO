#!/usr/bin/env bash
# K-Zero - controllo delle proposte (K-015): il deposito e' pubblico, e nel titolo di una proposta e nei messaggi dei suoi
# commit nuovi non si scrive nessuno dei quattro nomi (CLAUDE.md, Confini). Usa lo stesso strumento della prova
# deposito.nomi_e_file (tests/strumenti/controlla_deposito.cpp), che conosce solo le impronte dei nomi; i messaggi dicono
# dove e quale nome (a-d), mai il nome. Lo lancia il controllo GitHub (.github/workflows/nomi.yml); in locale, prima di
# aprire una proposta:  TITOLO="K-xxx: ..." tests/strumenti/controlla_proposta.sh build/tests/controlla_deposito origin/main HEAD
# Il titolo arriva in una variabile d'ambiente, non nel comando: un titolo scritto apposta non diventa un comando.
# Uso: TITOLO="<titolo>" controlla_proposta.sh <controlla_deposito> <base> <testa>   (commit di <testa> che <base> non ha)
# Uscita 0 se non c'e' nessun nome, 1 se ce n'e' almeno uno, 2 se mancano gli argomenti o un commit non si legge.
set -euo pipefail
if [ "$#" -ne 3 ] || [ -z "${TITOLO+x}" ]; then
  echo "Uso: TITOLO=\"<titolo>\" $0 <controlla_deposito> <base> <testa>" >&2
  exit 2
fi
strumento=$1
base=$2
testa=$3
if ! commit=$(git rev-list --reverse "$testa" --not "$base"); then
  echo "I commit tra $base e $testa non si leggono." >&2
  exit 2
fi
esito=0
printf '%s\n' "$TITOLO" | "$strumento" --testo "titolo della proposta" || esito=1
quanti=0
for c in $commit; do
  messaggio=$(git log -1 --format=%B "$c") || exit 2
  printf '%s\n' "$messaggio" | "$strumento" --testo "messaggio del commit ${c:0:12}" || esito=1
  quanti=$((quanti + 1))
done
if [ "$esito" -eq 0 ]; then
  echo "Titolo e messaggi di $quanti commit nuovi: nessun nome."
else
  echo "Ci sono nomi da togliere (CLAUDE.md, Confini): il titolo si cambia sulla pagina della proposta; un messaggio di"
  echo "commit gia' pubblicato resta nella storia, quindi si rifa' il commit prima di pubblicarlo (mai sui rami gia' uniti)."
fi
exit "$esito"
