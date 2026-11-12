#pragma once
// K-Zero API - avviso sui nomi liberi (K-018, docs/controlli.md "Testi comuni").
// I nomi, i codici e le note scritti da chi prepara i dati sono dati comuni: li vede chiunque, anche senza permessi e senza la
// cassaforte. Se dentro c'e' il nome di una persona dei dati o una parola che fa capire la salute, cassaforte e permessi non
// servono piu'. Questo controllo lo dice, con avvisi soltanto (non blocca niente):
// - K-CTR-136: il nome di una persona (personale, passeggero, committente privato) o di una casa (luogo privato) in un testo
//   comune: il nome intero (tutte le sue parole, una dopo l'altra in qualsiasi ordine, oppure attaccate in una parola sola) in
//   qualsiasi testo; una sola parola del nome di una persona (almeno 3 lettere) solo nei testi delle cose di quella persona
//   (l'elenco esatto in docs/controlli.md). Legge dati personali, anche quelli legati a un luogo sensibile: richiede tutti e due
//   i permessi (catalogo); rivela un dato personale, particolare per chi e' legato a un luogo sensibile (LegameSensibile). Il
//   messaggio dice dove sta il testo e il codice della persona o della casa, mai il nome come sta nei suoi dati.
// - K-CTR-137: una parola o un'espressione degli elenchi delle parole sulla salute (norme del paese, versione 4; aziende,
//   versione 4) in un testo comune. Un testo di un'azienda si confronta con gli elenchi delle sue norme e i suoi; un testo che
//   non e' di un'azienda (es. un luogo pubblico) con tutti gli elenchi del pacchetto. Non si controllano i file di regole
//   pubbliche (marcatura "x-kzero-regole-pubbliche": norme e regolamenti, scelta del supervisore) ne' i testi di un luogo segnato
//   sensibile e della scuola o del centro che sta li' (il loro nome e' pubblico e si vede comunque).
// - K-CTR-138: un luogo pubblico non segnato sensibile con una parola degli elenchi nel nome (nome o nome alternativo): forse e' un
//   luogo sensibile (al posto di K-CTR-137 per quei nomi).
// I testi controllati si ricavano dalle marcature degli schemi (classi.hpp, testi_comuni), mai da un elenco scritto a mano.
// Falsi allarmi (manifesto versione 3): un avviso di questi controlli si spegne per un testo preciso (controllo, file, campo,
// elemento e testo uguale carattere per carattere); un falso allarme che non spegne piu' niente e' la variante "falso allarme" del
// suo controllo, con la sua riservatezza.

#include <string>
#include <vector>

#include "controlli.hpp"
#include "lettore.hpp"

namespace kzero::api {

// Varianti dei messaggi (catalogo).
constexpr int kNomeIntero = 0;
constexpr int kNomeParola = 1;
constexpr int kNomeInteroLegato = 2;
constexpr int kNomeParolaLegato = 3;
constexpr int kNomeFalsoAllarme = 4;
constexpr int kSaluteTesto = 0;
constexpr int kSaluteFalsoAllarme = 1;
constexpr int kLuogoNonSegnato = 0;
constexpr int kLuogoVersione1 = 1;
constexpr int kLuogoFalsoAllarme = 2;

// Le parole di un testo per i confronti: la normalizzazione della ricerca dei luoghi (ricerca.hpp: senza maiuscole ne'
// accenti, punteggiatura e spazi come separatori), poi divise agli spazi e dove una lettera e una cifra si toccano ("ROSSI2" ->
// "ROSSI", "2"), come nel controllo del deposito (K-016).
std::vector<std::string> parole_del_testo(const std::string& testo);

// I problemi dei testi comuni di un pacchetto letto senza errori di forma, gia' senza quelli spenti dai falsi allarmi.
std::vector<kzero::Problem> controlla_testi_liberi(const Lettura& lettura);

}  // namespace kzero::api
