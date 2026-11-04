#!/usr/bin/env bash
# K-Zero - fuzzing (prove con dati casuali e deformati) di tutti i bersagli, per un tempo fisso ciascuno.
# Bersagli: un file del pacchetto alla volta (25 tipi di file, partendo dai file del demo, del demo "solo numeri", della
# copia del demo del K-007 per le versioni vecchie, del pacchetto di prova degli orari del K-011 (versioni 2), di quello dei
# servizi del K-012 (versioni nuove) e di tests/fuzz/semi;
# dal K-011 anche il resoconto degli orari di kzero ore), la riga di comando, date e orari; dal K-010 la
# cassaforte (il file, il testo decifrato delle sezioni), il registro (il file, il testo decifrato) e le password.
# Un guasto (memoria, comportamento non definito, risultato incoerente, attesa oltre 10 secondi) ferma il bersaglio:
# il file che lo provoca resta in <lavoro>/guasti/<bersaglio>/ e lo script esce con 1. Il file va corretto e messo in
# tests/fuzz/casi/<bersaglio>/ (diventa una prova normale, fuzz.casi.<bersaglio>).
#
# Uso: tests/fuzz/fuzz.sh <cartella della build con -DKZERO_FUZZ=ON> <secondi per bersaglio> [cartella di lavoro] [bersagli...]
#   es. controllo GitHub:   tests/fuzz/fuzz.sh build-fuzz 20
#       in locale, a lungo: tests/fuzz/fuzz.sh build-fuzz 3600 ~/kzero-fuzz
#       un solo bersaglio:  tests/fuzz/fuzz.sh build-fuzz 600 ~/kzero-fuzz pacchetto-corse.json
# I casi interessanti trovati restano in <lavoro>/corpus/<bersaglio>/: un giro successivo riparte da li'.
# KZERO_FUZZ_PARALLELI=<n>: n bersagli alla volta (es. il numero di processori).
set -u
radice="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(cd "$1" && pwd)"
secondi="$2"
lavoro="${3:-$build/fuzz-lavoro}"
shift 2
[ $# -gt 0 ] && shift
mkdir -p "$lavoro"
lavoro="$(cd "$lavoro" && pwd)"

file_formato=(pacchetto.json aziende.json norme.json luoghi.json tratti.json regolamenti.json festivita.json
  calendari.json scuole.json committenti.json contratti.json gruppi_servizi.json servizi.json corse.json mezzi.json
  rimesse.json tempi_sosta.json persone.json assenze.json fermi.json turni.json rotazioni.json servizi_fissi.json
  assegnazioni.json blocchi.json)
if [ $# -gt 0 ]; then
  bersagli=("$@")
else
  bersagli=()
  for f in "${file_formato[@]}"; do bersagli+=("pacchetto-$f"); done
  bersagli+=(parametri date-e-orari cassaforte cassaforte-voci registro registro-voci password)
fi
# Semi dei bersagli della cassaforte (K-010): preparati a ogni giro (chiavi casuali) da rigioca_fuzz.
semi_cassaforte="$lavoro/semi-cassaforte"
rm -rf "$semi_cassaforte"
if [ ! -x "$build/tests/rigioca_fuzz" ]; then
  echo "manca $build/tests/rigioca_fuzz, che prepara i semi della cassaforte: compilarlo con --target rigioca_fuzz" >&2
  exit 1
fi
"$build/tests/rigioca_fuzz" --scrivi-semi "$semi_cassaforte" || { echo "semi della cassaforte non scritti" >&2; exit 1; }

# Parole dei file di dati (nomi dei campi e valori fissi degli schemi): aiutano libFuzzer a scrivere JSON plausibile.
dizionario="$lavoro/k-zero.dict"
python3 - "$radice/docs/schema" > "$dizionario" <<'PY'
import glob, json, sys
parole = {'"schema"', '"schema_versione"', 'true', 'false', 'null', '{', '}', '[', ']', ':', ','}
def visita(n):
    if isinstance(n, dict):
        for k, v in n.items():
            if k == "properties" and isinstance(v, dict):
                parole.update('"%s"' % c for c in v)
            if k in ("enum", "const"):
                for x in (v if isinstance(v, list) else [v]):
                    parole.add(json.dumps(x))
            visita(v)
    elif isinstance(n, list):
        for x in n: visita(x)
for f in glob.glob(sys.argv[1] + "/*.schema.json"):
    visita(json.load(open(f)))
for p in sorted(parole):
    print('"%s"' % p.replace('\\', '\\\\').replace('"', '\\"'))
PY

riassunto="$lavoro/riassunto.txt"
touch "$riassunto"
paralleli="${KZERO_FUZZ_PARALLELI:-1}"

# Un bersaglio: prepara il corpus, lancia libFuzzer, scrive l'esito in <lavoro>/esito-<bersaglio>.txt.
prova_bersaglio() {
  local b="$1" corpus="$lavoro/corpus/$1" guasti="$lavoro/guasti/$1" programma f semi extra registro codice casi copertura
  local ambiente=()
  mkdir -p "$corpus" "$guasti"
  case "$b" in
    pacchetto-*)
      f="${b#pacchetto-}"
      programma="$build/tests/fuzz_pacchetto"
      ambiente=(KZERO_FILE="$f")
      for semi in "$radice/esempi/demo" "$radice/esempi/demo-numeri" "$radice/tests/esempi/k007-demo" "$radice/tests/esempi/k011-orari" \
                  "$radice/tests/esempi/k012-servizi"; do
        [ -f "$semi/$f" ] && cp "$semi/$f" "$corpus/seme-$(basename "$semi")-$f"
      done
      ;;
    parametri) programma="$build/tests/fuzz_parametri"; cp "$radice/tests/fuzz/semi/parametri/"* "$corpus/" ;;
    date-e-orari) programma="$build/tests/fuzz_date_e_orari"; cp "$radice/tests/fuzz/semi/date-e-orari/"* "$corpus/" ;;
    cassaforte|cassaforte-voci|registro|registro-voci|password)
      programma="$build/tests/fuzz_cassaforte"
      ambiente=(KZERO_BERSAGLIO="$b")
      cp "$semi_cassaforte/$b/"* "$corpus/" ;;
    *) echo "GUASTO $b: bersaglio sconosciuto" > "$lavoro/esito-$b.txt"; return 2 ;;
  esac
  # Semi in piu' (es. tempi_sosta.json, che il demo non ha) e casi gia' trovati.
  for extra in "$radice/tests/fuzz/semi/$b" "$radice/tests/fuzz/casi/$b"; do
    [ -d "$extra" ] && cp "$extra/"* "$corpus/" 2>/dev/null
  done
  [ -n "$(ls -A "$corpus")" ] || printf '{"schema": "%s", "schema_versione": 1}' "${f%.json}" > "$corpus/seme-minimo"
  registro="$lavoro/$b.log"
  # -timeout=10: un caso che impiega piu' di 10 secondi e' un guasto (il programma deve finire subito).
  # -rss_limit_mb=2048: un caso che chiede piu' di 2 GB di memoria e' un guasto.
  env "${ambiente[@]}" ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    "$programma" "$corpus" -max_total_time="$secondi" -timeout=10 -rss_limit_mb=2048 -max_len=262144 \
    -dict="$dizionario" -artifact_prefix="$guasti/" -print_final_stats=1 > "$registro" 2>&1
  codice=$?
  casi=$(grep -o 'stat::number_of_executed_units: *[0-9]*' "$registro" | grep -o '[0-9]*$' | tail -1)
  copertura=$(grep -o 'cov: [0-9]*' "$registro" | tail -1 | grep -o '[0-9]*')
  if [ $codice -ne 0 ]; then
    { echo "GUASTO $b (uscita $codice): casi in $guasti"
      grep -E "ERROR|GUASTO|runtime error|SUMMARY" "$registro" | head -5; } > "$lavoro/esito-$b.txt"
    return 1
  fi
  echo "ok $b: ${casi:-?} casi in ${secondi} s, copertura ${copertura:-?}" > "$lavoro/esito-$b.txt"
}

# I bersagli, KZERO_FUZZ_PARALLELI alla volta (di base uno).
for b in "${bersagli[@]}"; do
  while [ "$(jobs -rp | wc -l)" -ge "$paralleli" ]; do wait -n; done
  prova_bersaglio "$b" &
done
wait
esito=0
for b in "${bersagli[@]}"; do
  cat "$lavoro/esito-$b.txt" | tee -a "$riassunto"
  grep -q '^ok ' "$lavoro/esito-$b.txt" || esito=1
done
exit $esito
