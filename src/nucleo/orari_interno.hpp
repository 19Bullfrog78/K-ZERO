#pragma once
// K-Zero - orari di lavoro (K-011): strutture interne condivise tra orari.cpp (preparazione, piano, resoconto) e
// orari_regole.cpp (i tipi di regola). Non fa parte dell'interfaccia del nucleo: vedi orari.hpp.

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "orari.hpp"

namespace kzero::orari {

constexpr long long kDayMin = 1440;
constexpr long long kWeekMin = 7 * kDayMin;
// I mondi in cui si valuta ogni regola: 0 = certo (letture certe, solo i regimi certi, categorie presunte nel modo piu'
// favorevole); 1 = prudente (letture prudenti, anche i regimi da confermare, categorie presunte nel modo prudente); 2 = come il
// prudente ma con le letture certe. Il mondo 2 non da' problemi: serve solo a dire se un avviso viene dalle letture prudenti
// (motivo "lettura_prudente") o da altro (regimi, categorie, dati che mancano).
constexpr int kWorlds = 3;
constexpr int kMiddle = 2;
inline std::array<TimeCategory, kWorlds> cats(TimeCategory certain, TimeCategory prudent) { return {certain, prudent, prudent}; }

inline int fam(RuleFamily f) { return static_cast<int>(f); }
inline Certainty min_c(Certainty a, Certainty b) { return a < b ? a : b; }
inline Certainty max_c(Certainty a, Certainty b) { return a < b ? b : a; }
inline long long abs_min(Day d, Minutes m) { return static_cast<long long>(d) * kDayMin + m; }
inline Day day_of(long long abs) { return static_cast<Day>(abs >= 0 ? abs / kDayMin : -((-abs + kDayMin - 1) / kDayMin)); }

// Un pezzo della linea del tempo di una persona: un elemento di un turno oppure una sosta non scritta tra due elementi della
// stessa giornata. Tra due giornate non c'e' nessun pezzo: e' riposo (tempo libero).
struct Seg {
    long long a = 0, b = 0;                 // minuti assoluti, [a, b)
    std::array<TimeCategory, kWorlds> cat{};   // categoria in ogni mondo
    bool assumed = false;                   // categoria presunta (aziende.json versione 1, o tipo sconosciuto)
    bool unwritten = false;                 // sosta non scritta
    int day = 0;                            // indice della giornata (PersonPlan::days)
    int item = -1;                          // indice dell'elemento nella giornata; -1 = sosta non scritta
    const Code* place = nullptr;            // dove sta la persona (per il "fuori residenza")
};

// Un periodo di tempo libero (riposo o interruzione) della linea del tempo, con i bordi non noti (fuori dal periodo del
// pacchetto): la lunghezza vera e' almeno quella nota.
struct Free {
    long long a = 0, b = 0;
    bool open_left = false, open_right = false;
    long long len() const { return b - a; }
};

// Una violazione trovata in un mondo, prima di diventare un problema.
struct Breach {
    Check certain_check = Check::LawWorkViolated;   // nel mondo certo
    Check prudent_check = Check::LawWorkToConfirm;  // solo nel mondo prudente
    int variant = 0;
    Day day = 0;                                    // giorno dell'unita' (giornata, lunedi', inizio della finestra)
    std::vector<Day> days;                          // piu' giorni (finestre mobili), se servono
    std::vector<Param> params;                      // senza persona, giorni e motivi
    std::string key;                                // per non ripetere nel mondo prudente cio' che e' gia' un errore
    long long from = 0, to = 0;                     // minuti assoluti dell'unita' (per i motivi)
    bool day_assignment = false;                    // nell'unita' c'e' un'assegnazione del singolo giorno
    std::vector<std::string> extra_reasons;         // motivi gia' noti (eccezione, servizi occasionali...)
    const WorkRule* rule = nullptr;
    const Regime* regime = nullptr;
    bool work_measure = false;                      // senza regime: misura il lavoro contato dai regimi (contratto, lavoro massimo)
    bool chosen = false;                            // del regime per scelta dell'azienda (tachigrafo, K-011 aggiunta 1)
};

// Un regime per la persona: in quali giorni vale, per famiglia, e con quale certezza.
struct RegimeData {
    const Regime* regime = nullptr;
    const Regime* trip_conditions = nullptr;        // il regime da cui vengono le condizioni sulla corsa (come_regime)
    std::vector<std::array<Certainty, 5>> fam;      // per giorno del periodo (indice day - D0)
    std::vector<std::vector<ApplyReason>> why;      // per giorno
    std::vector<Certainty> day_level;               // per giorno, prima di estensioni ed esclusioni
    std::vector<Certainty> trigger_level;           // per giorno, dalle sole corse: cio' che fa scattare l'estensione
    // Per mondo e famiglia: quanti giorni validi fino a ogni giorno (per sapere in fretta se vale in un intervallo).
    std::array<std::array<std::vector<int>, 5>, kWorlds> valid_days;
    const WorkRule* count = nullptr;                // conteggio_lavoro del regime
    const WorkRule* daily_rest = nullptr;           // riposo giornaliero "entro 24 ore dalla fine del riposo"
    const WorkRule* weekly_rest = nullptr;
    int measure = -1;                               // indice della misura del lavoro (PersonCtx::measures)
    // Guida delle corse escluse (K-011 aggiunta 1, es. Reg. 561/2006 art. 6 c. 5). seg_level: per ogni pezzo, quanto vale il
    // regime per la corsa del pezzo (per un pezzo di guida senza passeggeri, per la corsa a cui serve; Certain se non c'entra).
    // Con own_drive le regole di guida e di interruzione del regime contano la guida con i loro pesi, mondo per mondo.
    std::vector<Certainty> seg_level;
    // Il regime "per scelta" dell'azienda (tachigrafo, K-011 aggiunta 1): vale anche sulle corse che la legge esclude; per
    // giorno la forza della scelta (0 nessuna, 1 preferenza, 2 limite). Le sue violazioni non sono la legge.
    bool chosen = false;
    std::vector<char> chosen_strength;
    bool own_drive = false;
    std::array<std::vector<int>, kWorlds> drive_weight;
    std::array<std::vector<long long>, kWorlds> drive_prefix;
};

// Una misura del lavoro (conteggio di un regime, o quella senza regime) nei due mondi: peso di ogni pezzo in centesimi
// (100 = contato per intero, 12 = il 12%).
struct Measure {
    const Regime* regime = nullptr;
    std::array<std::vector<int>, kWorlds> weight;
    std::array<std::vector<long long>, kWorlds> prefix;   // somme di peso x minuti
    std::array<std::vector<char>, kWorlds> stop_full;     // pezzo contato per intero perche' sosta breve
};

struct PersonCtx {
    const Index* index = nullptr;
    const Person* person = nullptr;
    const PersonPlan* plan = nullptr;
    const Norms* norms = nullptr;
    std::vector<Seg> segs;
    long long h0 = 0, h1 = 0;          // periodo noto: dall'inizio del primo giorno alla fine dell'ultimo del pacchetto
    Day d0 = 0, d1 = 0;                // periodo del pacchetto
    std::string base_municipality;     // comune della presa servizio
    std::vector<int> day_of_index;     // per giorno del periodo: indice della giornata, -1 se non c'e'
    std::vector<std::pair<std::size_t, std::size_t>> day_segs;   // per giornata: i suoi pezzi [primo, ultimo)
    std::vector<std::vector<int>> item_seg;   // per giornata e per elemento: l'indice del suo pezzo, -1 se non c'e' 
    std::vector<RegimeData> regimes;
    std::vector<Measure> measures;     // la prima e' quella senza regime
    std::array<std::vector<int>, kWorlds> drive_weight;         // 1 per i pezzi di guida
    std::array<std::vector<long long>, kWorlds> drive_prefix;   // minuti di guida
    int days() const { return d1 - d0 + 1; }
    bool in_period(Day d) const { return d >= d0 && d <= d1; }
    const PersonDay* pd(Day d) const {
        if (!in_period(d)) return nullptr;
        const int i = day_of_index[static_cast<std::size_t>(d - d0)];
        return i < 0 ? nullptr : &plan->days[static_cast<std::size_t>(i)];
    }
    long long start_abs(const PersonDay& p) const { return abs_min(p.day, p.start); }
    long long end_abs(const PersonDay& p) const { return abs_min(p.day, p.end); }
};

// Somma di peso x minuti dei pezzi nell'intervallo [a, b).
long long sum_in(const std::vector<Seg>& segs, const std::vector<long long>& prefix, const std::vector<int>& weight, long long a,
                 long long b);
// Periodi liberi nel mondo dato: le categorie che non interrompono il riposo sono l'interruzione e, se on_call_free, la
// reperibilita'.
std::vector<Free> free_periods(const PersonCtx& c, int world, bool on_call_free);

// Il regime vale per quella famiglia in quel giorno nel mondo dato (mondo certo: solo con certezza)?
bool applies(const RegimeData& rd, const PersonCtx& c, RuleFamily f, Day d, int world);
// Vale in almeno un giorno dell'intervallo di giorni [from, to]?
bool applies_any(const RegimeData& rd, const PersonCtx& c, RuleFamily f, Day from, Day to, int world);
// La giornata (o un'unita' di piu' giorni) ha un turno da un'assegnazione del singolo giorno?
bool has_day_assignment(const PersonCtx& c, Day from, Day to);

// Ogni tipo di regola: le violazioni trovate nel mondo dato e i giorni verificati solo in parte.
struct RuleOut {
    std::vector<Breach> breaches;
    std::vector<Day> partial;
    std::vector<Breach> notices;    // sempre avvisi (straordinario, riposo cumulato): il controllo e' in certain_check
    std::vector<Breach> near;       // giornate vicine al limite
};
void evaluate_rule(const PersonCtx& c, const RegimeData& rd, const WorkRule& rule, int world, const CompanySettings* settings,
                   RuleOut& out);

// Riposi giornalieri riconosciuti da una regola "entro 24 ore dalla fine del riposo" (per la guida tra due riposi).
std::vector<Free> recognized_rests(const PersonCtx& c, const RegimeData& rd, int world);

// Testo della fonte di una regola: "documento, punto".
std::string source_text(const RuleSource& s, const RuleSource& regime_source);

// Parametri comuni dei problemi di una regola di un regime (codice della regola, fonte), personali: la legge che vale per
// una persona dipende dal suo rapporto.
std::vector<Param> rule_params(const Regime& r, const WorkRule& w);
Param personal(Param p, std::string hidden = "");

}  // namespace kzero::orari
