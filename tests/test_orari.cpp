// K-Zero - prove del nucleo sugli orari di lavoro (K-011): i tipi di regola ai loro limiti (esattamente il limite e un minuto
// oltre, oltre la mezzanotte, il cambio di settimana, le finestre di 24 ore, i riposi frazionati e ridotti, la compensazione,
// i periodi di 24 ore), i conteggi del lavoro, le letture certa e prudente, le scelte dell'azienda e della persona, le
// verifiche parziali. Si parte dal pacchetto demo senza turni e con un regime di prova scritto qui: i numeri delle regole li
// scrive la prova, come li scriverebbe un utente nei dati (nessun valore di legge nel codice).
#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "controlli.hpp"
#include "controlli_catalogo.hpp"
#include "orari.hpp"
#include "pacchetto_demo.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

Source dove(const std::string& file, const std::string& puntatore) { return Source{file, puntatore, 0, 0}; }

// Un elemento della giornata: un'attivita' di un tipo di prova (G guida, A altre mansioni, D disponibilita', R reperibilita',
// P interruzione) in un luogo, dalle alle; oppure una corsa (codice) gia' nel pacchetto.
struct Elemento {
    std::string tipo, luogo, dalle, alle;
    std::string corsa;
};

Elemento el(const std::string& tipo, const std::string& dalle, const std::string& alle, const std::string& luogo = "L-RIMESSA-ALFA") {
    return {tipo, luogo, dalle, alle, ""};
}
Elemento corsa(const std::string& codice) { return {"", "", "", "", codice}; }

WorkRule regola(RuleType tipo, RuleFamily famiglia, RulePeriod periodo = RulePeriod::Workday) {
    static int n = 0;
    WorkRule w;
    w.code = "W-" + std::to_string(++n);
    w.type = tipo;
    w.family = famiglia;
    w.period = periodo;
    w.source.document = "regola di prova";
    w.src = dove("norme.json", "/norme/0/regimi/0/regole/0");
    return w;
}

WorkRule conteggio(std::vector<TimeCategory> categorie, std::optional<int> soste = std::nullopt) {
    WorkRule w = regola(RuleType::WorkCount, RuleFamily::Work);
    w.certain.measure_name = "lavoro di prova";
    w.certain.categories = std::move(categorie);
    w.certain.stops_up_to = soste;
    return w;
}

const std::vector<TimeCategory> kGAD = {TimeCategory::Driving, TimeCategory::OtherWork, TimeCategory::Availability};

class Prova {
public:
    explicit Prova(std::vector<WorkRule> regole) : p(demo()) {
        p.shifts.clear();
        p.rotations.clear();
        p.fixed.clear();
        p.day_assignments.clear();
        p.blocks.clear();
        p.absences.clear();
        p.stops.clear();
        Norms& n = per_codice(p.norms, "N-IT");
        n.regimes.clear();
        n.clock_changes.clear();
        Regime r;
        r.code = "R-PROVA";
        r.name = "Regime di prova";
        r.source.document = "regime di prova";
        r.rules = std::move(regole);
        r.src = dove("norme.json", "/norme/0/regimi/0");
        n.regimes.push_back(r);
        for (CompanySettings& s : p.company_settings) {
            s.choices.clear();
            s.return_key.reset();
            s.special_needs.clear();
            s.days_margin.reset();
            s.unwritten = UnwrittenStops{TimeCategory::Break, std::nullopt, TimeCategory::Break, {}};
            if (s.company.code != "ALFA") continue;
            s.activities.clear();
            const std::pair<const char*, TimeCategory> tipi[] = {{"G", TimeCategory::Driving},      {"A", TimeCategory::OtherWork},
                                                                 {"D", TimeCategory::Availability}, {"R", TimeCategory::OnCall},
                                                                 {"P", TimeCategory::Break}};
            for (const auto& [codice, tempo] : tipi) {
                ActivityType a;
                a.code = codice;
                a.name = codice;
                a.uses_vehicle = false;
                a.time = tempo;
                a.src = dove("aziende.json", "/aziende/0/attivita/0");
                s.activities.push_back(a);
            }
        }
        for (Person& x : p.persons) {
            x.choices.clear();
            x.return_key.reset();
            x.prior_days.reset();
            x.hours.clear();
        }
    }

    Regime& regime() { return per_codice(p.norms, "N-IT").regimes.front(); }

    // Una giornata della persona: un turno con un calendario di quel solo giorno, con un servizio fisso (turno ricorrente)
    // oppure un'assegnazione del singolo giorno.
    void giornata(const std::string& data, const std::vector<Elemento>& elementi, bool ricorrente = true, const std::string& persona = "P-A") {
        const std::string n = std::to_string(p.shifts.size());
        Calendar c;
        c.code = "CAL-PROVA-" + n;
        c.range = {giorno(data), giorno(data)};
        for (int g = 0; g < 7; ++g) c.days.add(static_cast<Weekday>(g));
        c.src = dove("calendari.json", "/calendari/" + std::to_string(p.calendars.size()));
        p.calendars.push_back(c);
        Shift s;
        const std::string pnt = "/turni/" + n;
        s.code = "T-PROVA-" + n;
        s.company = Ref{"ALFA", dove("turni.json", pnt + "/azienda")};
        s.role = ShiftRole::Driver;
        s.calendar = Ref{c.code, dove("turni.json", pnt + "/calendario")};
        s.vehicle_mode = ShiftVehicleMode::None;
        s.vehicle_src = dove("turni.json", pnt + "/mezzo");
        s.start_place = Ref{"L-RIMESSA-ALFA", dove("turni.json", pnt + "/inizio")};
        s.end_place = Ref{"L-RIMESSA-ALFA", dove("turni.json", pnt + "/fine")};
        for (std::size_t k = 0; k < elementi.size(); ++k) {
            ShiftItem it;
            if (!elementi[k].corsa.empty()) {
                it.is_trip = true;
                it.trip.trip = Ref{elementi[k].corsa, {}};
            } else {
                it.is_trip = false;
                it.activity.type = Ref{elementi[k].tipo, {}};
                it.activity.place = Ref{elementi[k].luogo, {}};
                it.activity.start = ora(elementi[k].dalle);
                it.activity.end = ora(elementi[k].alle);
            }
            it.src = dove("turni.json", pnt + "/elementi/" + std::to_string(k));
            s.items.push_back(it);
        }
        s.src = dove("turni.json", pnt);
        if (std::any_of(elementi.begin(), elementi.end(), [](const Elemento& e) { return !e.corsa.empty(); })) {
            s.vehicle_mode = ShiftVehicleMode::Specific;
            s.vehicle = Ref{"ALFA-05", dove("turni.json", pnt + "/mezzo/codice")};
        }
        p.shifts.push_back(s);
        if (ricorrente) {
            FixedAssignment f;
            f.code = "F-PROVA-" + n;
            f.shift = Ref{s.code, {}};
            f.person = Ref{persona, {}};
            f.calendar = Ref{c.code, {}};
            f.src = dove("servizi_fissi.json", "/servizi_fissi/" + std::to_string(p.fixed.size()));
            p.fixed.push_back(f);
        } else {
            DayAssignment a;
            a.date = giorno(data);
            a.shift = Ref{s.code, {}};
            a.person = Ref{persona, {}};
            a.src = dove("assegnazioni.json", "/assegnazioni/" + std::to_string(p.day_assignments.size()));
            p.day_assignments.push_back(a);
        }
    }

    // La stessa giornata in piu' giorni di fila.
    void giornate(const std::string& dal, int quanti, const std::vector<Elemento>& elementi) {
        for (int i = 0; i < quanti; ++i) giornata(text_from_day(giorno(dal) + i), elementi);
    }

    std::vector<Problem> problemi(Check c) const {
        std::vector<Problem> out;
        for (const Problem& x : check_package(p))
            if (x.check == c) out.push_back(x);
        return out;
    }

    std::vector<Problem> tutti() const { return check_package(p); }

    HoursReport resoconto(const std::string& dal, const std::string& al) const {
        HoursRequest r;
        r.report = true;
        r.from = giorno(dal);
        r.to = giorno(al);
        HoursReport rep;
        check_package(p, &r, &rep);
        return rep;
    }

    Package p;
};

const Param* parametro(const Problem& x, const std::string& nome) {
    for (const Param& q : x.params)
        if (q.name == nome) return &q;
    return nullptr;
}

long long misurato(const Problem& x) {
    const Param* q = parametro(x, "misurato");
    return q ? q->number : -1;
}

bool con_motivo(const Problem& x, const std::string& motivo) {
    const Param* q = parametro(x, "motivi");
    return q && ("," + q->text + ",").find("," + motivo + ",") != std::string::npos;
}

int variante(const std::vector<Problem>& v, int k) {
    return static_cast<int>(std::count_if(v.begin(), v.end(), [k](const Problem& x) { return x.variant == k; }));
}

std::string elenco(const std::vector<Problem>& v) {
    std::string s;
    for (const Problem& x : v) {
        s += "\n  " + api::definizione_controllo(x.check).codice + " v" + std::to_string(x.variant);
        for (const Param& q : x.params) s += " " + q.name + "=" + (q.text.empty() ? std::to_string(q.number) : q.text);
    }
    return s.empty() ? " nessuno" : s;
}

}  // namespace

// --- lavoro_massimo ------------------------------------------------------------------------------------------------------

TEST(Orari, lavoro_massimo_al_limite) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-10-05", {el("G", "07:00", "17:00")});   // esattamente 10 ore
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty()) << elenco(a.problemi(Check::LawWorkViolated));
    Prova b({conteggio(kGAD), max});
    b.giornata("2026-10-05", {el("G", "07:00", "17:01")});   // un minuto oltre
    const auto v = b.problemi(Check::LawWorkViolated);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(v[0].variant, 0);
    EXPECT_EQ(misurato(v[0]), 601);
}

// Un turno oltre la mezzanotte resta della giornata in cui comincia.
TEST(Orari, lavoro_massimo_oltre_la_mezzanotte) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-10-05", {el("G", "20:00", "30:01")});
    const auto v = a.problemi(Check::LawWorkViolated);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(parametro(v[0], "giorni")->days, std::vector<Day>{giorno("2026-10-05")});
}

// La settimana va da lunedi' 00:00 a domenica 24:00: un turno a cavallo divide i minuti tra le due settimane.
TEST(Orari, settimana_da_lunedi_a_domenica) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work, RulePeriod::Week);
    max.certain.max_minutes = 60;
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-10-11", {el("G", "23:00", "25:00")});   // domenica: 60 minuti per settimana
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty()) << elenco(a.problemi(Check::LawWorkViolated));
    Prova b({conteggio(kGAD), max});
    b.giornata("2026-10-11", {el("G", "22:59", "25:00")});   // 61 minuti nella settimana del 5 ottobre
    const auto v = b.problemi(Check::LawWorkViolated);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(v[0].variant, 2);
    EXPECT_EQ(parametro(v[0], "giorni")->days, std::vector<Day>{giorno("2026-10-05")});
    EXPECT_EQ(misurato(v[0]), 61);
}

// Il normale e il massimo: tra i due uno straordinario (avviso, con l'obbligo scritto nei dati); oltre il massimo un errore.
TEST(Orari, straordinario_tra_normale_e_massimo) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.normal_minutes = 480;
    max.certain.max_minutes = 600;
    max.overtime = Overtime{"denuncia di prova", "vietato di prova", false};
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-10-05", {el("G", "07:00", "16:00")});           // 9 ore, turno ricorrente
    a.giornata("2026-10-06", {el("G", "07:00", "16:00")}, false);    // 9 ore, assegnazione del singolo giorno
    const auto v = a.problemi(Check::LawOvertime);
    ASSERT_EQ(v.size(), 2u) << elenco(v);
    EXPECT_EQ(variante(v, 1), 1);   // ricorrente: "vietato"
    EXPECT_EQ(variante(v, 0), 1);   // del giorno: "denuncia"
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
}

// --- guida_continua ------------------------------------------------------------------------------------------------------

namespace {
WorkRule guida_continua_561() {
    WorkRule w = regola(RuleType::ContinuousDriving, RuleFamily::Driving);
    w.certain.max_minutes = 270;
    w.certain.resets = {BreakPattern{{45}, false}, BreakPattern{{15, 30}, false}, BreakPattern{{15, 15}, true}};
    w.certain.categories = {TimeCategory::Break};
    return w;
}
}  // namespace

TEST(Orari, guida_continua_al_limite) {
    Prova a({guida_continua_561()});
    a.giornata("2026-10-05", {el("G", "06:00", "10:30")});   // 4 ore e 30
    EXPECT_TRUE(a.problemi(Check::LawDrivingViolated).empty());
    Prova b({guida_continua_561()});
    b.giornata("2026-10-05", {el("G", "06:00", "10:31")});
    const auto v = b.problemi(Check::LawDrivingViolated);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 271);
}

// 15 seguiti da 30 azzerano il conto; 30 seguiti da 15 no (l'ordine conta); 15 + 15 solo per i servizi occasionali.
TEST(Orari, guida_continua_interruzioni_in_parti) {
    Prova a({guida_continua_561()});
    a.giornata("2026-10-05", {el("G", "06:00", "08:30"), el("P", "08:30", "08:45"), el("G", "08:45", "09:15"), el("P", "09:15", "09:45"),
                              el("G", "09:45", "11:30")});
    EXPECT_TRUE(a.problemi(Check::LawDrivingViolated).empty()) << elenco(a.problemi(Check::LawDrivingViolated));
    Prova b({guida_continua_561()});
    b.giornata("2026-10-05", {el("G", "06:00", "08:30"), el("P", "08:30", "09:00"), el("G", "09:00", "09:30"), el("P", "09:30", "09:45"),
                              el("G", "09:45", "11:30")});
    EXPECT_EQ(b.problemi(Check::LawDrivingViolated).size(), 1u);
    Prova c({guida_continua_561()});
    c.giornata("2026-10-05", {el("G", "06:00", "08:30"), el("P", "08:30", "08:45"), el("G", "08:45", "09:15"), el("P", "09:15", "09:30"),
                              el("G", "09:30", "11:15")});
    EXPECT_EQ(c.problemi(Check::LawDrivingViolated).size(), 1u);   // 15 + 15 senza servizi occasionali: niente
}

// 15 + 15 vale per i servizi occasionali del regime (Reg. 561/2006 art. 7: gite e noleggi): con corse di un servizio
// "altro" e il regime che dice che "altro" e' occasionale, il conto si azzera; senza quella voce no.
TEST(Orari, guida_continua_15_piu_15_nei_servizi_occasionali) {
    const auto prova = [](bool occasionali) {
        Prova p({guida_continua_561()});
        if (occasionali) p.regime().occasional = {ServiceKind::Other};
        const auto tratta = [&](const std::string& codice, const std::string& da, const std::string& dalle, const std::string& a,
                                const std::string& alle) {
            Trip t;
            t.code = codice;
            t.services = {Ref{"S-GITA", {}}};
            t.calendar = Ref{"CAL-SEMPRE", {}};
            t.passages.resize(2);
            t.passages[0].place = Ref{da, {}};
            t.passages[0].time = ora(dalle);
            t.passages[1].place = Ref{a, {}};
            t.passages[1].time = ora(alle);
            t.passages[1].loaded_km = 60.0;
            t.src = dove("corse.json", "/corse/" + std::to_string(p.p.trips.size()));
            p.p.trips.push_back(t);
        };
        tratta("C-PROVA-1", "L-RIMESSA-ALFA", "06:00", "F-LAGOBELLO", "08:30");
        tratta("C-PROVA-2", "F-LAGOBELLO", "08:45", "F-PRADELLO", "09:15");
        tratta("C-PROVA-3", "F-PRADELLO", "09:30", "L-RIMESSA-ALFA", "11:15");
        p.giornata("2026-10-05", {corsa("C-PROVA-1"), el("P", "08:30", "08:45", "F-LAGOBELLO"), corsa("C-PROVA-2"),
                                  el("P", "09:15", "09:30", "F-PRADELLO"), corsa("C-PROVA-3")});
        return p.problemi(Check::LawDrivingViolated);
    };
    EXPECT_TRUE(prova(true).empty()) << elenco(prova(true));
    EXPECT_EQ(prova(false).size(), 1u);
}

// La disponibilita' non e' un'interruzione per il 561 (art. 4 d): il conto non si azzera.
TEST(Orari, guida_continua_la_disponibilita_non_azzera) {
    Prova a({guida_continua_561()});
    a.giornata("2026-10-05", {el("G", "06:00", "09:00"), el("D", "09:00", "10:00"), el("G", "10:00", "11:31")});
    EXPECT_EQ(a.problemi(Check::LawDrivingViolated).size(), 1u);
    Prova b({guida_continua_561()});
    b.giornata("2026-10-05", {el("G", "06:00", "09:00"), el("P", "09:00", "10:00"), el("G", "10:00", "11:31")});
    EXPECT_TRUE(b.problemi(Check::LawDrivingViolated).empty());
}

// --- guida_massima tra due riposi, con l'estensione --------------------------------------------------------------------

namespace {
WorkRule riposo_561(bool ridotto = true) {
    WorkRule w = regola(RuleType::DailyRest, RuleFamily::Rest);
    w.measure = RestMeasure::From24hEndRest;
    w.certain.min_minutes = 660;
    w.certain.split = {180, 540};
    if (ridotto) {
        w.certain.reduced_min = 540;
        w.certain.reduced_times = 3;
    }
    return w;
}
WorkRule settimanale_561() {
    WorkRule w = regola(RuleType::WeeklyRest, RuleFamily::Rest);
    w.certain.min_minutes = 2700;
    w.certain.reduced_min = 1440;
    return w;
}
}  // namespace

TEST(Orari, guida_massima_con_estensione_due_volte) {
    WorkRule g = regola(RuleType::MaxDriving, RuleFamily::Driving, RulePeriod::BetweenRests);
    g.certain.max_minutes = 540;
    g.certain.extension_max = 600;
    g.certain.extension_times = 2;
    Prova a({riposo_561(), g});
    a.giornata("2026-10-05", {el("G", "06:00", "16:00")});   // 10 ore: prima estensione
    a.giornata("2026-10-06", {el("G", "06:00", "16:00")});   // seconda
    a.giornata("2026-10-07", {el("G", "06:00", "15:00")});   // 9 ore
    EXPECT_TRUE(a.problemi(Check::LawDrivingViolated).empty()) << elenco(a.problemi(Check::LawDrivingViolated));
    Prova b({riposo_561(), g});
    b.giornata("2026-10-05", {el("G", "06:00", "16:00")});
    b.giornata("2026-10-06", {el("G", "06:00", "16:00")});
    b.giornata("2026-10-07", {el("G", "06:00", "16:00")});   // terza estensione della settimana
    b.giornata("2026-10-08", {el("G", "06:00", "16:01")});   // oltre l'estensione
    const auto v = b.problemi(Check::LawDrivingViolated);
    EXPECT_EQ(variante(v, 2), 1) << elenco(v);
    EXPECT_EQ(variante(v, 1), 1) << elenco(v);
}

// --- riposo_giornaliero -------------------------------------------------------------------------------------------------

// Riposo frazionato: 3 + 9 si', 9 + 3 no (senza riposo ridotto).
TEST(Orari, riposo_frazionato_nell_ordine) {
    Prova a({riposo_561(false)});
    a.giornata("2026-10-05", {el("G", "06:00", "09:00"), el("G", "12:00", "21:00")});   // 3 ore libere, poi 9 la notte
    a.giornata("2026-10-06", {el("G", "06:00", "07:00")});
    EXPECT_TRUE(a.problemi(Check::LawDailyRestViolated).empty()) << elenco(a.problemi(Check::LawDailyRestViolated));
    Prova b({riposo_561(false)});
    b.giornata("2026-10-05", {el("G", "06:00", "07:00"), el("G", "16:00", "23:00")});   // 9 ore libere, poi 3 la notte
    b.giornata("2026-10-06", {el("G", "02:00", "07:00")});
    const auto v = b.problemi(Check::LawDailyRestViolated);
    EXPECT_EQ(variante(v, 1), 1) << elenco(v);
}

// Il quarto riposo ridotto tra due riposi settimanali e' una violazione; il terzo no.
TEST(Orari, quarto_riposo_ridotto) {
    Prova a({riposo_561(), settimanale_561()});
    a.giornate("2026-10-05", 4, {el("G", "07:00", "21:00")});   // tre riposi di 10 ore
    EXPECT_TRUE(a.problemi(Check::LawDailyRestViolated).empty()) << elenco(a.problemi(Check::LawDailyRestViolated));
    Prova b({riposo_561(), settimanale_561()});
    b.giornate("2026-10-05", 5, {el("G", "07:00", "21:00")});   // quattro
    const auto v = b.problemi(Check::LawDailyRestViolated);
    ASSERT_EQ(variante(v, 2), 1) << elenco(v);
}

// Nelle 24 ore dall'inizio della giornata (L. 138 art. 7): esattamente il minimo si', un minuto meno no.
TEST(Orari, riposo_nelle_24_ore_dall_inizio) {
    WorkRule r = regola(RuleType::DailyRest, RuleFamily::Rest);
    r.measure = RestMeasure::From24hStart;
    r.certain.min_minutes = 540;
    Prova a({r});
    a.giornata("2026-10-05", {el("G", "06:00", "21:00")});   // libero dalle 21:00 alle 06:00: 9 ore
    a.giornata("2026-10-06", {el("G", "06:00", "07:00")});
    EXPECT_TRUE(a.problemi(Check::LawDailyRestViolated).empty()) << elenco(a.problemi(Check::LawDailyRestViolated));
    Prova b({r});
    b.giornata("2026-10-05", {el("G", "06:00", "21:01")});
    b.giornata("2026-10-06", {el("G", "06:00", "07:00")});
    const auto v = b.problemi(Check::LawDailyRestViolated);
    ASSERT_EQ(variante(v, 0), 1) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 539);
}

// --- riposo_settimanale -------------------------------------------------------------------------------------------------

// Un riposo a cavallo di due settimane conta in una sola: l'altra settimana resta senza.
TEST(Orari, riposo_settimanale_a_cavallo_conta_una_volta) {
    Prova a({settimanale_561()});
    a.giornate("2026-09-28", 12, {el("G", "08:00", "18:00")});   // dal lunedi' 28 settembre al venerdi' 9 ottobre
    a.giornata("2026-10-10", {el("G", "06:00", "12:00")});      // sabato: poi 45 ore fino a lunedi' 12 alle 09:00
    a.giornate("2026-10-12", 14, {el("G", "09:00", "18:00")});
    const auto v = a.problemi(Check::LawWeeklyRestViolated);
    EXPECT_EQ(variante(v, 0), 1) << elenco(v);
}

// Il riposo settimanale comincia al piu' tardi dopo sei periodi di 24 ore dalla fine del precedente.
TEST(Orari, settimo_periodo_di_24_ore) {
    WorkRule w = settimanale_561();
    w.certain.within_24h_periods = 6;
    const auto settimana = [&](const std::string& fine_domenica) {
        Prova p({w});
        p.giornata("2026-10-02", {el("G", "08:00", "18:00")});   // venerdi', poi riposo fino a lunedi' 06:00
        p.giornate("2026-10-05", 6, {el("G", "06:00", "20:00")});
        p.giornata("2026-10-11", {el("G", "05:00", fine_domenica)});
        return p.problemi(Check::LawWeeklyRestViolated);
    };
    EXPECT_EQ(variante(settimana("06:00"), 2), 0) << elenco(settimana("06:00"));   // comincia esattamente dopo 6 x 24 ore
    EXPECT_EQ(variante(settimana("06:01"), 2), 1) << elenco(settimana("06:01"));
}

// La riduzione si compensa entro la fine della terza settimana dopo, tutta insieme e attaccata a un riposo: nella terza
// settimana si', nella quarta no.
TEST(Orari, compensazione_terza_e_quarta_settimana) {
    WorkRule w = settimanale_561();
    w.certain.compensation_weeks = 3;
    w.certain.compensation_attached = 540;
    // Il riposo ridotto della settimana del 5 ottobre serve: senza di lui il riposo settimanale dopo quello che finisce
    // lunedi' 5 alle 08:00 comincerebbe oltre i 6 periodi di 24 ore. Quindi e' un riposo settimanale e va compensato.
    w.certain.within_24h_periods = 6;
    const auto prova = [&](bool nella_terza) {
        Prova p({w});
        const std::vector<Elemento> giorno_pieno = {el("G", "08:00", "18:00")};
        // Settimana del 5 ottobre: riposo settimanale ridotto di 24 ore (21 ore da compensare), da sabato 06:00 a domenica 06:00.
        p.giornate("2026-10-05", 5, giorno_pieno);
        p.giornata("2026-10-10", {el("G", "00:00", "06:00")});
        p.giornata("2026-10-11", {el("G", "06:00", "18:00")});
        // Settimane del 12 e del 19 ottobre: riposi regolari di 45 ore, da sabato 18:00 a lunedi' 15:00.
        for (const char* lunedi : {"2026-10-12", "2026-10-19"}) {
            p.giornata(lunedi, {el("G", std::string(lunedi) == "2026-10-12" ? "08:00" : "15:00", "18:00")});
            p.giornate(text_from_day(giorno(lunedi) + 1), 5, giorno_pieno);
        }
        if (nella_terza) {
            // Settimana del 26 ottobre (la terza dopo): 66 ore (45 + 21) da giovedi' 18:00 a domenica 12:00.
            p.giornata("2026-10-26", {el("G", "15:00", "18:00")});
            p.giornate("2026-10-27", 3, giorno_pieno);
            p.giornata("2026-11-01", {el("G", "12:00", "18:00")});
            p.giornate("2026-11-02", 5, giorno_pieno);
        } else {
            // Nella settimana del 2 novembre (la quarta dopo): troppo tardi.
            p.giornata("2026-10-26", {el("G", "15:00", "18:00")});
            p.giornate("2026-10-27", 5, giorno_pieno);
            p.giornata("2026-11-02", {el("G", "15:00", "18:00")});
            p.giornate("2026-11-03", 3, giorno_pieno);
            p.giornata("2026-11-08", {el("G", "12:00", "18:00")});
            p.giornate("2026-11-09", 5, giorno_pieno);
        }
        return p.problemi(Check::LawWeeklyRestViolated);
    };
    EXPECT_EQ(variante(prova(true), 3), 0) << elenco(prova(true));
    EXPECT_EQ(variante(prova(false), 3), 1) << elenco(prova(false));
}

// Un riposo di 42 ore a meta' settimana, in settimane che hanno gia' il riposo regolare del fine settimana e dove i 6
// periodi di 24 ore sono rispettati anche senza di lui: e' solo un riposo lungo, niente da compensare (prova di scala).
TEST(Orari, riposo_ridotto_in_piu_non_si_compensa) {
    WorkRule w = settimanale_561();
    w.certain.compensation_weeks = 3;
    w.certain.compensation_attached = 540;
    w.certain.within_24h_periods = 6;
    w.certain.two_weeks_regular = 1;
    Prova p({w});
    for (const char* lunedi : {"2026-10-05", "2026-10-12", "2026-10-19", "2026-10-26", "2026-11-02"}) {
        const Day l = giorno(lunedi);
        for (const Day d : {l, l + 1, l + 3, l + 4}) p.giornata(text_from_day(d), {el("G", "06:00", "12:00")});
    }
    const auto v = p.problemi(Check::LawWeeklyRestViolated);
    EXPECT_TRUE(v.empty()) << elenco(v);
    EXPECT_TRUE(p.problemi(Check::LawWeeklyRestToConfirm).empty()) << elenco(p.problemi(Check::LawWeeklyRestToConfirm));
}

// --- lavoro_continuo, lavoro_notturno, giornate_massime, ampiezza_massima, giorni_consecutivi ---------------------------

TEST(Orari, lavoro_continuo_e_pause) {
    WorkRule w = regola(RuleType::ContinuousWork, RuleFamily::Work);
    w.certain.max_minutes = 360;
    w.certain.pauses = {PauseNeed{360, 30}, PauseNeed{540, 45}};
    w.certain.fraction_min = 15;
    Prova a({conteggio({TimeCategory::Driving, TimeCategory::OtherWork}), w});
    a.giornata("2026-10-05", {el("G", "07:00", "13:00"), el("P", "13:00", "13:30"), el("G", "13:30", "15:00")});
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty()) << elenco(a.problemi(Check::LawWorkViolated));
    Prova b({conteggio({TimeCategory::Driving, TimeCategory::OtherWork}), w});
    b.giornata("2026-10-05", {el("G", "07:00", "13:01"), el("P", "13:01", "13:31"), el("G", "13:31", "15:00")});
    EXPECT_EQ(variante(b.problemi(Check::LawWorkViolated), 5), 1);
    Prova c({conteggio({TimeCategory::Driving, TimeCategory::OtherWork}), w});   // pause di 10 minuti: non contano
    c.giornata("2026-10-05", {el("G", "07:00", "10:00"), el("P", "10:00", "10:10"), el("G", "10:10", "13:00"), el("P", "13:00", "13:10"),
                              el("G", "13:10", "15:00")});
    const auto v = c.problemi(Check::LawWorkViolated);
    EXPECT_EQ(variante(v, 5), 1) << elenco(v);
    EXPECT_EQ(variante(v, 6), 1) << elenco(v);
}

// Notte con due letture: certa 03:00-04:00, prudente 00:00-07:00. Lavoro alle 03:30 = notturno certo (errore); alle 05:45
// solo prudente (avviso da confermare).
TEST(Orari, notturno_certo_e_prudente) {
    WorkRule w = regola(RuleType::NightWork, RuleFamily::Night);
    w.certain.band_from = ora("03:00");
    w.certain.band_to = ora("04:00");
    w.certain.max_minutes = 600;
    RuleParams pr = w.certain;
    pr.band_from = ora("00:00");
    pr.band_to = ora("07:00");
    w.prudent = pr;
    Prova a({conteggio({TimeCategory::Driving, TimeCategory::OtherWork}), w});
    a.giornata("2026-10-05", {el("G", "03:30", "14:31")});
    EXPECT_EQ(variante(a.problemi(Check::LawWorkViolated), 7), 1);
    Prova b({conteggio({TimeCategory::Driving, TimeCategory::OtherWork}), w});
    b.giornata("2026-10-05", {el("G", "05:45", "16:46")});
    EXPECT_TRUE(b.problemi(Check::LawWorkViolated).empty());
    const auto v = b.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(variante(v, 7), 1) << elenco(v);
    EXPECT_TRUE(con_motivo(v[0], "lettura_prudente"));
}

TEST(Orari, giornate_massime_con_le_giornate_di_prima) {
    WorkRule w = regola(RuleType::MaxDays, RuleFamily::Days);
    w.certain.max_days = 10;
    w.certain.years = 3;
    // Giornate di prima contate da quando sono tutte dentro il triennio: violazione certa.
    Prova a({w});
    per_codice(a.p.persons, "P-A").prior_days = PriorDays{5, giorno("2024-01-01"), {}};
    a.giornate("2026-10-05", 6, {el("A", "08:00", "12:00")});
    EXPECT_EQ(a.problemi(Check::LawWorkViolated).size(), 1u) << elenco(a.problemi(Check::LawWorkViolated));
    // Contate da prima dell'inizio del triennio: forse fuori. Solo un avviso, con il motivo.
    Prova b({w});
    per_codice(b.p.persons, "P-A").prior_days = PriorDays{5, giorno("2023-01-01"), {}};
    b.giornate("2026-10-05", 6, {el("A", "08:00", "12:00")});
    EXPECT_TRUE(b.problemi(Check::LawWorkViolated).empty());
    const auto v = b.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "giornate_prima_del_pacchetto"));
    // Senza le giornate di prima: verifica parziale, mai "regolare".
    Prova c({w});
    c.giornate("2026-10-05", 6, {el("A", "08:00", "12:00")});
    EXPECT_TRUE(c.problemi(Check::LawWorkViolated).empty());
    EXPECT_EQ(c.problemi(Check::PartialVerification).size(), 1u);
}

TEST(Orari, ampiezza_e_giorni_consecutivi) {
    WorkRule amp = regola(RuleType::MaxSpan, RuleFamily::Work);
    amp.certain.max_minutes = 720;
    WorkRule cons = regola(RuleType::ConsecutiveDays, RuleFamily::Days);
    cons.certain.max_days = 6;
    Prova a({amp, cons});
    a.giornata("2026-10-05", {el("A", "07:00", "08:00"), el("A", "18:00", "19:00")});   // 12 ore
    a.giornate("2026-10-06", 5, {el("A", "08:00", "09:00")});                         // 6 giorni di fila
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty()) << elenco(a.problemi(Check::LawWorkViolated));
    Prova b({amp, cons});
    b.giornata("2026-10-05", {el("A", "07:00", "08:00"), el("A", "18:00", "19:01")});
    b.giornate("2026-10-06", 6, {el("A", "08:00", "09:00")});
    const auto v = b.problemi(Check::LawWorkViolated);
    EXPECT_EQ(variante(v, 9), 1) << elenco(v);
    EXPECT_EQ(variante(v, 10), 1) << elenco(v);
}

// --- Conteggi del lavoro ------------------------------------------------------------------------------------------------

namespace {
long long lavoro(const HoursReport& r, const std::string& persona, std::size_t misura = 0) {
    for (const PersonHours& ph : r.persons)
        if (ph.person->code == persona && !ph.days.empty() && ph.days.front().work.size() > misura)
            return (ph.days.front().work[misura].certain_x100 + 99) / 100;
    return -1;
}
}  // namespace

// Soste fino a 30 minuti contate come lavoro (L. 138 art. 6 a): 30 si', 31 no.
TEST(Orari, soste_di_30_e_di_31_minuti) {
    Prova a({conteggio(kGAD, 30)});
    a.giornata("2026-10-05", {el("G", "07:00", "09:00"), el("G", "09:30", "11:30")});
    EXPECT_EQ(lavoro(a.resoconto("2026-10-05", "2026-10-05"), "P-A"), 270);
    Prova b({conteggio(kGAD, 30)});
    b.giornata("2026-10-05", {el("G", "07:00", "09:00"), el("G", "09:31", "11:31")});
    EXPECT_EQ(lavoro(b.resoconto("2026-10-05", "2026-10-05"), "P-A"), 240);
}

// Il 12% del tempo inoperoso fuori residenza (fuori dal comune della presa servizio).
TEST(Orari, dodici_per_cento_fuori_residenza) {
    WorkRule c = conteggio(kGAD, 30);
    c.certain.idle_away = IdleShare{{TimeCategory::Break}, 12};
    Prova a({c});
    a.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("P", "08:00", "09:40", "F-LAGOBELLO"), el("G", "09:40", "10:40")});
    EXPECT_EQ(lavoro(a.resoconto("2026-10-05", "2026-10-05"), "P-A"), 132);
    Prova b({c});
    b.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("P", "08:00", "09:40"), el("G", "09:40", "10:40")});   // a Valfonda
    EXPECT_EQ(lavoro(b.resoconto("2026-10-05", "2026-10-05"), "P-A"), 120);
}

// La disponibilita' conta per un regime (es. la L. 138) e non per un altro (es. il D.Lgs. 234).
TEST(Orari, disponibilita_conta_per_un_regime_e_non_per_l_altro) {
    Prova a({conteggio(kGAD)});
    Regime secondo;
    secondo.code = "R-PROVA-2";
    secondo.name = "Secondo regime di prova";
    secondo.source.document = "prova";
    secondo.rules = {conteggio({TimeCategory::Driving, TimeCategory::OtherWork})};
    per_codice(a.p.norms, "N-IT").regimes.push_back(secondo);
    a.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("D", "08:00", "09:00"), el("G", "09:00", "10:00")});
    const HoursReport r = a.resoconto("2026-10-05", "2026-10-05");
    EXPECT_EQ(lavoro(r, "P-A", 0), 180);
    EXPECT_EQ(lavoro(r, "P-A", 1), 120);
}

// --- Letture da confermare, regimi da confermare, categorie presunte ----------------------------------------------------

TEST(Orari, regola_da_confermare_solo_avvisi) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    max.to_confirm = true;
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-10-05", {el("G", "07:00", "18:00")});
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
    const auto v = a.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "regola_da_confermare"));
}

TEST(Orari, regime_da_confermare_solo_avvisi) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    a.regime().to_confirm = true;
    a.giornata("2026-10-05", {el("G", "07:00", "18:00")});
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
    const auto v = a.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "regime_da_confermare"));
}

// Un rapporto da confermare (es. il titolare per la L. 138): oltre il limite un avviso, non un errore.
TEST(Orari, titolare_oltre_il_limite_avviso) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    a.regime().employments = Applies<EmploymentKind>{true, {EmploymentKind::FullTime}, {EmploymentKind::Owner}};
    a.giornata("2026-10-05", {el("G", "07:00", "18:00")}, true, "P-C");   // P-C e' titolare
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
    const auto v = a.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "rapporto"));
    // Quale legge vale per la persona dipende dal rapporto: la regola e i valori sono dati personali.
    EXPECT_EQ(parametro(v[0], "regola")->sensitivity, Param::Sensitivity::Personal);
    EXPECT_EQ(parametro(v[0], "misurato")->sensitivity, Param::Sensitivity::Personal);
}

// aziende.json versione 1: tipo senza categoria. Per prudenza altre mansioni (mai interruzione): le violazioni che vengono
// solo da questa prudenza sono avvisi.
TEST(Orari, categoria_presunta_solo_avvisi) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    for (CompanySettings& s : a.p.company_settings)
        if (s.company.code == "ALFA")
            for (ActivityType& t : s.activities)
                if (t.code == "P") t.time.reset();
    a.giornata("2026-10-05", {el("G", "07:00", "12:00"), el("P", "12:00", "14:00"), el("G", "14:00", "18:00")});   // 9 ore + 2 di tipo P
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
    const auto v = a.problemi(Check::LawWorkToConfirm);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "categorie_presunte"));
    EXPECT_EQ(a.problemi(Check::TimeCategoryMissing).size(), 1u);
}

// Senza nessuna legge sugli orari per un dipendente: un avviso, mai "regolare".
TEST(Orari, dipendente_senza_legge) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    a.regime().employments = Applies<EmploymentKind>{true, {EmploymentKind::Owner}, {}};
    a.giornata("2026-10-05", {el("G", "07:00", "18:00")});
    EXPECT_EQ(a.problemi(Check::EmployeeWithoutLaw).size(), 1u);
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
}

// Una finestra che esce dal periodo del pacchetto (la prima settimana, se il periodo comincia di martedi'): verifica
// parziale, mai "regolare".
TEST(Orari, finestra_prima_del_periodo) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work, RulePeriod::Week);
    max.certain.max_minutes = 2880;
    Prova a({conteggio(kGAD), max});
    a.giornata("2026-09-01", {el("G", "07:00", "12:00")});   // martedi', primo giorno del pacchetto
    EXPECT_TRUE(a.problemi(Check::LawWorkViolated).empty());
    const auto v = a.problemi(Check::PartialVerification);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(parametro(v[0], "giorni")->days, std::vector<Day>{giorno("2026-09-01")});
}

// --- Scelte dell'azienda e della persona -------------------------------------------------------------------------------

namespace {
WorkChoice scelta(int minuti, ChoiceStrength forza) {
    WorkChoice c;
    c.type = RuleType::MaxWork;
    c.period = RulePeriod::Workday;
    c.value = minuti;
    c.strength = forza;
    c.src = dove("aziende.json", "/aziende/0/scelte_orari/0");
    return c;
}
CompanySettings& alfa(Package& p) {
    for (CompanySettings& s : p.company_settings)
        if (s.company.code == "ALFA") return s;
    throw std::logic_error("ALFA");
}
}  // namespace

TEST(Orari, scelte_limite_preferenza_e_persona) {
    // Limite rigido su un turno ricorrente: errore; sull'assegnazione del singolo giorno: avviso.
    Prova a({conteggio(kGAD)});
    alfa(a.p).choices = {scelta(480, ChoiceStrength::Limit)};
    a.giornata("2026-10-05", {el("G", "07:00", "15:20")});
    a.giornata("2026-10-06", {el("G", "07:00", "15:20")}, false);
    EXPECT_EQ(a.problemi(Check::ChoiceLimitViolated).size(), 1u);
    const auto v = a.problemi(Check::ChoiceNotRespected);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(con_motivo(v[0], "assegnazione_del_giorno"));
    // Preferenza: avviso.
    Prova b({conteggio(kGAD)});
    alfa(b.p).choices = {scelta(480, ChoiceStrength::Preference)};
    b.giornata("2026-10-05", {el("G", "07:00", "15:20")});
    EXPECT_TRUE(b.problemi(Check::ChoiceLimitViolated).empty());
    ASSERT_EQ(b.problemi(Check::ChoiceNotRespected).size(), 1u);
    EXPECT_TRUE(con_motivo(b.problemi(Check::ChoiceNotRespected)[0], "preferenza"));
    // Il valore della persona vince su quello dell'azienda.
    Prova c({conteggio(kGAD)});
    alfa(c.p).choices = {scelta(480, ChoiceStrength::Limit)};
    per_codice(c.p.persons, "P-A").choices = {scelta(540, ChoiceStrength::Limit)};
    c.giornata("2026-10-05", {el("G", "07:00", "15:20")});
    EXPECT_TRUE(c.problemi(Check::ChoiceLimitViolated).empty());
    per_codice(c.p.persons, "P-A").choices = {scelta(450, ChoiceStrength::Limit)};
    const auto w = c.problemi(Check::ChoiceLimitViolated);
    ASSERT_EQ(w.size(), 1u);
    EXPECT_EQ(w[0].variant, 8);   // della persona
}

// La legge resta sopra: una scelta piu' larga della legge si accetta con un avviso, e la legge si controlla lo stesso.
TEST(Orari, legge_piu_severa_della_scelta) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max});
    alfa(a.p).choices = {scelta(660, ChoiceStrength::Limit)};
    a.giornata("2026-10-05", {el("G", "07:00", "17:20")});
    EXPECT_EQ(a.problemi(Check::ChoiceLooserThanLaw).size(), 1u);
    EXPECT_EQ(a.problemi(Check::LawWorkViolated).size(), 1u);
    EXPECT_TRUE(a.problemi(Check::ChoiceLimitViolated).empty());
}

// --- Cambio dell'ora, stessi dati = stessi problemi --------------------------------------------------------------------

TEST(Orari, turno_che_attraversa_il_cambio_dell_ora) {
    Prova a({conteggio(kGAD)});
    per_codice(a.p.norms, "N-IT").clock_changes = {ClockChange{giorno("2026-10-25"), ora("03:00"), ora("02:00"), {}}};
    a.giornata("2026-10-24", {el("G", "22:00", "26:30")});   // fino alle 02:30 del 25: attraversa
    a.giornata("2026-10-25", {el("G", "07:00", "09:00")});
    const auto v = a.problemi(Check::ClockChangeCrossed);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(parametro(v[0], "giorni")->days, std::vector<Day>{giorno("2026-10-24")});
}

TEST(Orari, stessi_dati_stessi_problemi) {
    WorkRule max = regola(RuleType::MaxWork, RuleFamily::Work);
    max.certain.max_minutes = 600;
    Prova a({conteggio(kGAD), max, guida_continua_561(), riposo_561(), settimanale_561()});
    a.giornate("2026-10-05", 10, {el("G", "06:00", "11:00"), el("P", "11:00", "11:20"), el("G", "11:20", "17:30")});
    const auto x = a.tutti();
    const auto y = a.tutti();
    ASSERT_EQ(x.size(), y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        EXPECT_EQ(x[i].check, y[i].check);
        EXPECT_EQ(x[i].variant, y[i].variant);
        EXPECT_EQ(x[i].where.pointer, y[i].where.pointer);
    }
}
