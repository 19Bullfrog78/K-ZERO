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
    std::string verso;   // trasferimento: il luogo dove arriva
};

Elemento el(const std::string& tipo, const std::string& dalle, const std::string& alle, const std::string& luogo = "L-RIMESSA-ALFA") {
    return {tipo, luogo, dalle, alle, "", ""};
}
Elemento corsa(const std::string& codice) { return {"", "", "", "", codice, ""}; }
Elemento trasferimento(const std::string& da, const std::string& dalle, const std::string& alle, const std::string& verso) {
    return {"G", da, dalle, alle, "", verso};
}

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
                if (!elementi[k].verso.empty()) it.activity.to_place = Ref{elementi[k].verso, {}};
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

// --- K-011 aggiunta 1: conteggi possibili, prove ai limiti ---------------------------------------------------------------

namespace {
DatedHours massime_al_giorno(double ore) {
    DatedHours h;
    h.max = HoursLimits{ore, std::nullopt, std::nullopt};
    h.src = dove("persone.json", "/persone/0/ore/0");
    return h;
}
Regime secondo_regime(std::vector<WorkRule> regole) {
    Regime r;
    r.code = "R-PROVA-2";
    r.name = "Secondo regime di prova";
    r.source.document = "prova";
    r.rules = std::move(regole);
    r.src = dove("norme.json", "/norme/0/regimi/1");
    return r;
}
}  // namespace

// Due regimi che valgono con certezza nello stesso giorno e contano il lavoro in modo diverso (es. la L. 138 con la
// disponibilita' e il D.Lgs. 234 senza): per le ore massime del contratto, nella lettura certa il conteggio piu' basso
// (errore solo se tutti i conteggi superano il limite), nella prudente il piu' alto.
TEST(Orari, contratto_con_due_regimi_certi_il_conteggio_piu_basso) {
    const auto prova = [](double massime) {
        Prova p({conteggio(kGAD)});
        per_codice(p.p.norms, "N-IT").regimes.push_back(secondo_regime({conteggio({TimeCategory::Driving, TimeCategory::OtherWork})}));
        per_codice(p.p.persons, "P-A").hours = {massime_al_giorno(massime)};
        p.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("D", "08:00", "09:00"), el("G", "09:00", "10:00")});   // 3 ore e 2 ore
        return p;
    };
    const Prova a = prova(2.5);   // 2 ore e 30: solo il conteggio con la disponibilita' (3 ore) supera
    EXPECT_TRUE(a.problemi(Check::ContractHoursExceeded).empty()) << elenco(a.problemi(Check::ContractHoursExceeded));
    const auto v = a.problemi(Check::ContractHoursToConfirm);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 180);
    const Prova b = prova(1.5);   // 1 ora e 30: tutti e due i conteggi superano
    const auto e = b.problemi(Check::ContractHoursExceeded);
    ASSERT_EQ(e.size(), 1u) << elenco(e);
    EXPECT_EQ(misurato(e[0]), 120);   // il piu' basso
}

// Nessun regime certo, un regime da confermare che conta meno del lavoro "senza regime": il conteggio certo e' il piu' basso
// tra quelli possibili (anche quello del regime da confermare), quindi nessun errore, ma un avviso con il motivo. Caso
// dell'arbitro sul demo: P-A con T-DIS (L. 138, Reg. 561 e D.Lgs. 234 da confermare), K-CTR-120 e non K-CTR-119.
TEST(Orari, contratto_regime_da_confermare_nessun_errore_non_certo) {
    const auto prova = [](double massime) {
        Prova p({conteggio({TimeCategory::Driving})});
        p.regime().employments = Applies<EmploymentKind>{true, {EmploymentKind::FullTime}, {EmploymentKind::Owner}};
        per_codice(p.p.persons, "P-C").hours = {massime_al_giorno(massime)};   // P-C e' titolare: il regime e' da confermare
        p.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("D", "08:00", "09:00"), el("G", "09:00", "10:00")}, true, "P-C");
        return p;
    };
    const Prova a = prova(2.5);   // regime 2 ore, senza regime 3 ore
    EXPECT_TRUE(a.problemi(Check::ContractHoursExceeded).empty()) << elenco(a.problemi(Check::ContractHoursExceeded));
    const auto v = a.problemi(Check::ContractHoursToConfirm);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 180);
    EXPECT_TRUE(con_motivo(v[0], "rapporto")) << elenco(v);
    const Prova b = prova(1.5);   // tutti i conteggi superano: errore
    const auto e = b.problemi(Check::ContractHoursExceeded);
    ASSERT_EQ(e.size(), 1u) << elenco(e);
    EXPECT_EQ(misurato(e[0]), 120);
}

// Lo stesso per il limite rigido di una scelta dell'azienda sul lavoro (K-CTR-121): errore solo se e' certo.
TEST(Orari, scelta_limite_sul_lavoro_errore_solo_se_certo) {
    const auto prova = [](int minuti) {
        Prova p({conteggio({TimeCategory::Driving})});
        p.regime().employments = Applies<EmploymentKind>{true, {EmploymentKind::FullTime}, {EmploymentKind::Owner}};
        alfa(p.p).choices = {scelta(minuti, ChoiceStrength::Limit)};
        p.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("D", "08:00", "09:00"), el("G", "09:00", "10:00")}, true, "P-C");
        return p;
    };
    const Prova a = prova(150);
    EXPECT_TRUE(a.problemi(Check::ChoiceLimitViolated).empty()) << elenco(a.problemi(Check::ChoiceLimitViolated));
    ASSERT_EQ(a.problemi(Check::ChoiceNotRespected).size(), 1u) << elenco(a.problemi(Check::ChoiceNotRespected));
    EXPECT_TRUE(con_motivo(a.problemi(Check::ChoiceNotRespected)[0], "rapporto"));
    const Prova b = prova(90);
    ASSERT_EQ(b.problemi(Check::ChoiceLimitViolated).size(), 1u) << elenco(b.problemi(Check::ChoiceLimitViolated));
    EXPECT_EQ(misurato(b.problemi(Check::ChoiceLimitViolated)[0]), 120);
}

// Soste non scritte al limite (aziende.json, impostazioni.soste_non_scritte): fino a 30 minuti la prima categoria
// (disponibilita'), 31 minuti quella "oltre" (interruzione).
TEST(Orari, soste_non_scritte_di_30_e_di_31_minuti) {
    const auto prova = [](const std::string& ripresa) {
        Prova p({conteggio(kGAD)});
        alfa(p.p).unwritten = UnwrittenStops{TimeCategory::Availability, 30, TimeCategory::Break, {}};
        p.giornata("2026-10-05", {el("G", "07:00", "08:00"), el("G", ripresa, "10:00")});
        const HoursReport r = p.resoconto("2026-10-05", "2026-10-05");
        return r.persons.at(0).days.at(0).minutes;
    };
    const auto a = prova("08:30");
    EXPECT_EQ(a[static_cast<std::size_t>(TimeCategory::Availability)], 30);
    EXPECT_EQ(a[static_cast<std::size_t>(TimeCategory::Break)], 0);
    const auto b = prova("08:31");
    EXPECT_EQ(b[static_cast<std::size_t>(TimeCategory::Availability)], 0);
    EXPECT_EQ(b[static_cast<std::size_t>(TimeCategory::Break)], 31);
}

// Due riposi settimanali ridotti di fila (Reg. 561/2006 art. 8 c. 6: in due settimane consecutive almeno un riposo regolare),
// con i riposi tutti dentro la loro settimana: un riposo a cavallo di due settimane si potrebbe contare nell'altra (art. 8
// c. 9) e il caso sparirebbe. Esempio dell'arbitro.
TEST(Orari, due_riposi_settimanali_ridotti_di_fila) {
    WorkRule w = settimanale_561();
    w.certain.two_weeks_regular = 1;
    w.certain.within_24h_periods = 6;
    const auto prova = [&](bool seconda_ridotta) {
        Prova p({w});
        const std::vector<Elemento> pieno = {el("G", "08:00", "18:00")};
        const auto con_regolare = [&](const std::string& lunedi) {   // da venerdi' 18:00 a domenica 15:00: 45 ore
            p.giornate(lunedi, 5, pieno);
            p.giornata(text_from_day(giorno(lunedi) + 6), {el("G", "15:00", "20:00")});
        };
        const auto con_ridotto = [&](const std::string& lunedi) {    // da sabato 06:00 a domenica 12:00: 30 ore
            p.giornate(lunedi, 5, pieno);
            p.giornata(text_from_day(giorno(lunedi) + 5), {el("G", "00:00", "06:00")});
            p.giornata(text_from_day(giorno(lunedi) + 6), {el("G", "12:00", "20:00")});
        };
        con_regolare("2026-09-28");
        con_ridotto("2026-10-05");
        if (seconda_ridotta) con_ridotto("2026-10-12");
        else con_regolare("2026-10-12");
        con_regolare("2026-10-19");
        con_regolare("2026-10-26");
        return p.problemi(Check::LawWeeklyRestViolated);
    };
    const auto v = prova(true);
    EXPECT_EQ(variante(v, 1), 1) << elenco(v);
    EXPECT_EQ(v.size(), 1u) << elenco(v);
    const auto r = prova(false);
    EXPECT_TRUE(r.empty()) << elenco(r);
}

// Persona in prestito (K-011 aggiunta 1, punto 4): P-A di Alfa fa un turno di Beta. Valgono le scelte sugli orari
// (scelte_orari) della sua azienda (Alfa), le soste non scritte e la chiave del rientro dell'azienda del turno (Beta).
// E' il comportamento di oggi, scritto in docs/formato-dati.md e DECISIONI.md: Mauro decidera' se cambiarlo (modo consorzio).
TEST(Orari, persona_in_prestito_scelte_della_sua_azienda) {
    Prova p({conteggio(kGAD)});
    alfa(p.p).choices = {scelta(480, ChoiceStrength::Limit)};
    for (CompanySettings& s : p.p.company_settings) {
        if (s.company.code != "BETA") continue;
        s.choices = {scelta(300, ChoiceStrength::Limit)};
        s.unwritten = UnwrittenStops{TimeCategory::Availability, std::nullopt, TimeCategory::Break, {}};
        s.activities = alfa(p.p).activities;
    }
    p.giornata("2026-10-05", {el("G", "07:00", "11:00"), el("G", "11:30", "15:50")});   // 8 ore e 20 di guida, 30 minuti di sosta
    p.p.shifts.back().company.code = "BETA";
    const auto limite = p.problemi(Check::ChoiceLimitViolated);
    ASSERT_EQ(limite.size(), 1u) << elenco(limite);   // il limite di Alfa (8 ore), non quello di Beta (5 ore)
    EXPECT_EQ(parametro(limite[0], "limite")->number, 480);
    // La sosta non scritta di 30 minuti segue Beta (disponibilita'), non Alfa (interruzione): 8 ore e 50 di lavoro.
    EXPECT_EQ(misurato(limite[0]), 530);
}

// --- K-011 aggiunta 1: guida delle corse escluse (Reg. 561/2006 art. 6 c. 5) --------------------------------------------

namespace {
// Un regime come il 561: esclude le linee fino a 50 km, e la guida delle corse escluse conta come altre mansioni nella
// lettura certa e come guida nella prudente. Le corse: linea di 60 km (non esclusa) e di 10 km (esclusa).
Prova con_corse_escluse() {
    Prova p({guida_continua_561()});
    RegimeExclusion e;
    e.kinds = {ServiceKind::Line};
    e.route_up_to_km = 50.0;
    p.regime().exclusions = {e};
    ExcludedDriving g;
    g.certain = TimeCategory::OtherWork;
    g.prudent = TimeCategory::Driving;
    g.source.document = "regola di prova";
    p.regime().excluded_driving = g;
    const auto tratta = [&](const std::string& codice, const std::string& da, const std::string& dalle, const std::string& a,
                            const std::string& alle, double km) {
        Trip t;
        t.code = codice;
        t.services = {Ref{"S-LIN-B610", {}}};
        t.calendar = Ref{"CAL-SEMPRE", {}};
        t.passages.resize(2);
        t.passages[0].place = Ref{da, {}};
        t.passages[0].time = ora(dalle);
        t.passages[1].place = Ref{a, {}};
        t.passages[1].time = ora(alle);
        t.passages[1].loaded_km = km;
        t.src = dove("corse.json", "/corse/" + std::to_string(p.p.trips.size()));
        p.p.trips.push_back(t);
    };
    tratta("C-LUNGA-1", "F-LAGOBELLO", "06:00", "F-PRADELLO", "08:00", 60.0);    // 2 ore, non esclusa
    tratta("C-CORTA", "F-PRADELLO", "08:00", "F-LAGOBELLO", "09:00", 10.0);      // 1 ora, esclusa
    tratta("C-LUNGA-2", "F-LAGOBELLO", "09:00", "F-PRADELLO", "11:30", 60.0);    // 2 ore e 30, non esclusa
    tratta("C-LUNGA-3", "F-LAGOBELLO", "09:00", "F-PRADELLO", "11:31", 60.0);    // 2 ore e 31, non esclusa
    tratta("C-LUNGA-4", "F-LAGOBELLO", "06:00", "F-PRADELLO", "10:01", 60.0);    // 4 ore e 1 minuto, non esclusa
    tratta("C-LUNGA-5", "F-LAGOBELLO", "09:30", "F-PRADELLO", "14:00", 60.0);    // 4 ore e 30, non esclusa
    return p;
}
}  // namespace

// Una corsa esclusa tra due tratti di guida non esclusa: nella lettura certa e' altre mansioni, quindi non aumenta la guida di
// fila (2 ore + 2 ore e 30 = 4 ore e 30, al limite: nessun errore) e non la azzera (2 ore + 2 ore e 31 = un minuto oltre:
// errore). Nella lettura prudente e' guida: un avviso con il motivo.
TEST(Orari, corsa_esclusa_tra_due_tratti_di_guida) {
    Prova a = con_corse_escluse();
    a.giornata("2026-10-05", {corsa("C-LUNGA-1"), corsa("C-CORTA"), corsa("C-LUNGA-2")});
    EXPECT_TRUE(a.problemi(Check::LawDrivingViolated).empty()) << elenco(a.problemi(Check::LawDrivingViolated));
    const auto v = a.problemi(Check::LawDrivingToConfirm);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 330);
    EXPECT_TRUE(con_motivo(v[0], "guida_esclusa")) << elenco(v);
    Prova b = con_corse_escluse();
    b.giornata("2026-10-05", {corsa("C-LUNGA-1"), corsa("C-CORTA"), corsa("C-LUNGA-3")});
    const auto e = b.problemi(Check::LawDrivingViolated);
    ASSERT_EQ(e.size(), 1u) << elenco(e);
    EXPECT_EQ(misurato(e[0]), 271);
}

// La guida senza passeggeri e' legata alla corsa a cui serve: il trasferimento che arriva dove comincia una corsa non esclusa
// e' guida (30 minuti + 4 ore e 1 minuto: errore); quello che parte da dove finisce una corsa esclusa e' altre mansioni come
// la corsa; uno che non si sa a cosa serve, nella lettura certa altre mansioni e nella prudente guida.
TEST(Orari, trasferimenti_legati_alla_corsa_a_cui_servono) {
    Prova a = con_corse_escluse();
    a.giornata("2026-10-05", {trasferimento("L-RIMESSA-ALFA", "05:30", "06:00", "F-LAGOBELLO"), corsa("C-LUNGA-4")});
    const auto e = a.problemi(Check::LawDrivingViolated);
    ASSERT_EQ(e.size(), 1u) << elenco(e);
    EXPECT_EQ(misurato(e[0]), 271);
    // Con la guida delle corse escluse altre mansioni anche nella lettura prudente: il trasferimento che parte da dove finisce
    // la corsa esclusa e' legato a lei (altre mansioni), quindi solo le 4 ore e 30 della corsa dopo: nessun problema.
    const auto solo_altre_mansioni = [] {
        Prova p = con_corse_escluse();
        p.regime().excluded_driving->prudent = TimeCategory::OtherWork;
        return p;
    };
    Prova b = solo_altre_mansioni();
    b.giornata("2026-10-05", {corsa("C-CORTA"), trasferimento("F-LAGOBELLO", "09:00", "09:30", "L-RIMESSA-ALFA"), corsa("C-LUNGA-5")});
    EXPECT_TRUE(b.problemi(Check::LawDrivingViolated).empty()) << elenco(b.problemi(Check::LawDrivingViolated));
    EXPECT_TRUE(b.problemi(Check::LawDrivingToConfirm).empty()) << elenco(b.problemi(Check::LawDrivingToConfirm));
    // Un trasferimento che non va dove comincia la corsa dopo e non parte da dove finisce quella prima: non si sa a cosa serve,
    // da confermare (nella lettura certa altre mansioni, nella prudente guida): un avviso.
    Prova c = solo_altre_mansioni();
    c.giornata("2026-10-05", {corsa("C-CORTA"), trasferimento("L-RIMESSA-ALFA", "09:00", "09:30", "L-RIMESSA-ALFA"), corsa("C-LUNGA-5")});
    EXPECT_TRUE(c.problemi(Check::LawDrivingViolated).empty()) << elenco(c.problemi(Check::LawDrivingViolated));
    const auto v = c.problemi(Check::LawDrivingToConfirm);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(misurato(v[0]), 300);
    EXPECT_TRUE(con_motivo(v[0], "guida_esclusa")) << elenco(v);
}

// --- K-011 aggiunta 1: OUT o tachigrafo (Mauro, 28/10) -------------------------------------------------------------------

namespace {
ModeEntry voce(TripModeKind modo) {
    ModeEntry e;
    e.mode = modo;
    e.src = dove("aziende.json", "/aziende/0/modo_corse/per_tipo_servizio/0");
    return e;
}
ModeEntry per_tipo(ServiceKind k, TripModeKind modo) {
    ModeEntry e = voce(modo);
    e.kind = k;
    return e;
}
ModeEntry per_servizio(const std::string& s, TripModeKind modo) {
    ModeEntry e = voce(modo);
    e.service = Ref{s, {}};
    e.src = dove("aziende.json", "/aziende/0/modo_corse/servizi/0");
    return e;
}
ModeEntry per_corsa(const std::string& c, TripModeKind modo) {
    ModeEntry e = voce(modo);
    e.trip = Ref{c, {}};
    e.src = dove("aziende.json", "/aziende/0/modo_corse/corse/0");
    return e;
}
ModeJustification giustificazione(const std::string& pnt) {
    ModeJustification j;
    j.reason = "motivo di prova";
    j.who = "ufficio di prova";
    j.when = giorno("2026-10-28");
    j.src = dove("aziende.json", pnt);
    return j;
}
TripModes modi(std::vector<ModeEntry> tipi, std::vector<ModeEntry> servizi = {}, std::vector<ModeEntry> corse = {}) {
    TripModes m;
    m.regime = Ref{"R-PROVA", {}};
    m.by_kind = std::move(tipi);
    m.by_service = std::move(servizi);
    m.by_trip = std::move(corse);
    m.src = dove("aziende.json", "/aziende/0/modo_corse");
    return m;
}

// Un regime come il 561: per i mezzi oltre 9 persone, esclude la linea fino a 50 km e lo scolastico fino a 50 km (certi), i
// disabili insieme allo scolastico (certi) e i disabili da soli (da confermare); la guida delle corse escluse e' altre
// mansioni nella lettura certa. Le corse si scrivono con i loro servizi e i km.
struct Prova561 {
    Prova p;
    explicit Prova561(std::vector<WorkRule> regole = {guida_continua_561()}) : p(std::move(regole)) {
        Regime& r = p.regime();
        r.vehicle = VehicleCondition{9, false};
        const auto escl = [](std::vector<ServiceKind> k, std::vector<ServiceKind> insieme, bool dc) {
            RegimeExclusion e;
            e.kinds = std::move(k);
            e.together_with = std::move(insieme);
            e.route_up_to_km = 50.0;
            e.to_confirm = dc;
            return e;
        };
        r.exclusions = {escl({ServiceKind::Line}, {}, false), escl({ServiceKind::School}, {}, false),
                        escl({ServiceKind::Disability}, {ServiceKind::School}, false), escl({ServiceKind::Disability}, {}, true)};
        ExcludedDriving g;
        g.source.document = "regola di prova";
        r.excluded_driving = g;
    }
    void corsa_(const std::string& codice, std::vector<std::string> servizi, const std::string& dalle, const std::string& alle, double km) {
        Trip t;
        t.code = codice;
        for (const std::string& s : servizi) t.services.push_back(Ref{s, {}});
        t.calendar = Ref{"CAL-SEMPRE", {}};
        t.passages.resize(2);
        t.passages[0].place = Ref{"F-LAGOBELLO", {}};
        t.passages[0].time = ora(dalle);
        t.passages[1].place = Ref{"F-PRADELLO", {}};
        t.passages[1].time = ora(alle);
        t.passages[1].loaded_km = km;
        t.src = dove("corse.json", "/corse/" + std::to_string(p.p.trips.size()));
        p.p.trips.push_back(t);
    }
    // Una giornata con una corsa di 4 ore e 31 minuti (oltre le 4 ore e 30 di guida di fila).
    void lunga(const std::string& codice, std::vector<std::string> servizi, double km, bool ricorrente = true) {
        corsa_(codice, std::move(servizi), "06:00", "10:31", km);
        p.giornata("2026-10-05", {corsa(codice)}, ricorrente);
    }
};
}  // namespace

// Vince la scelta piu' precisa: la corsa, poi il servizio, poi il tipo di servizio (come per chi fa il turno).
TEST(Orari, modo_della_corsa_vince_la_scelta_piu_precisa) {
    Prova561 a;
    a.corsa_("C-M", {"S-LIN-B610"}, "06:00", "07:00", 10.0);
    const Trip& t = a.p.p.trips.back();
    const Index idx(a.p.p);
    CompanySettings s;
    s.trip_modes = modi({per_tipo(ServiceKind::Line, TripModeKind::Out)});
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).mode, TripModeKind::Out);
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).from, TripMode::From::Kind);
    s.trip_modes = modi({per_tipo(ServiceKind::Line, TripModeKind::Out)}, {per_servizio("S-LIN-B610", TripModeKind::Tachograph)});
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).mode, TripModeKind::Tachograph);
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).from, TripMode::From::Service);
    s.trip_modes = modi({per_tipo(ServiceKind::Line, TripModeKind::Out)}, {per_servizio("S-LIN-B610", TripModeKind::Tachograph)},
                        {per_corsa("C-M", TripModeKind::Out)});
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).mode, TripModeKind::Out);
    EXPECT_EQ(trip_mode(t, &s, idx, giorno("2026-10-05")).from, TripMode::From::Trip);
    s.trip_modes = modi({});
    EXPECT_FALSE(trip_mode(t, &s, idx, giorno("2026-10-05")).any);   // senza scelta decide la legge
}

// Corsa dell'infanzia con un bambino disabile in piu' (asilo + disabili): con le scelte uguali (out) e' out; con scelte
// diverse decide l'impostazione dell'azienda (di base la piu' prudente, il tachigrafo), con un avviso.
TEST(Orari, asilo_piu_disabili_scelte_uguali_e_diverse) {
    Prova561 a;
    alfa(a.p.p).trip_modes = modi({per_tipo(ServiceKind::School, TripModeKind::Out), per_tipo(ServiceKind::Disability, TripModeKind::Out)});
    a.lunga("C-MISTA", {"S-ASILO", "S-DIS"}, 30.0);
    const Index ia(a.p.p);
    EXPECT_EQ(trip_mode(a.p.p.trips.back(), &alfa(a.p.p), ia, giorno("2026-10-05")).mode, TripModeKind::Out);
    EXPECT_TRUE(a.p.problemi(Check::ModeMixed).empty());
    EXPECT_TRUE(a.p.problemi(Check::LawDrivingViolated).empty());       // scolastico + disabili fino a 50 km: escluso con certezza
    EXPECT_TRUE(a.p.problemi(Check::LawDrivingToConfirm).empty()) << elenco(a.p.problemi(Check::LawDrivingToConfirm));
    Prova561 b;
    alfa(b.p.p).trip_modes = modi({per_tipo(ServiceKind::School, TripModeKind::Out), per_tipo(ServiceKind::Disability, TripModeKind::Tachograph)});
    b.lunga("C-MISTA", {"S-ASILO", "S-DIS"}, 30.0);
    const Index ib(b.p.p);
    const TripMode m = trip_mode(b.p.p.trips.back(), &alfa(b.p.p), ib, giorno("2026-10-05"));
    EXPECT_EQ(m.mode, TripModeKind::Tachograph);
    EXPECT_TRUE(m.mixed);
    const auto v = b.p.problemi(Check::ModeMixed);
    ASSERT_EQ(v.size(), 1u) << elenco(v);
    EXPECT_EQ(v[0].variant, 0);
    EXPECT_EQ(b.p.problemi(Check::ChosenRegimeNotRespected).size(), 1u) << elenco(b.p.tutti());   // il 561 per scelta
    alfa(b.p.p).trip_modes->mixed = MixedModes::Out;   // l'impostazione dell'azienda: out
    EXPECT_EQ(b.p.problemi(Check::ModeMixed).at(0).variant, 1);
    EXPECT_TRUE(b.p.problemi(Check::ChosenRegimeNotRespected).empty());
}

// K-011 aggiunta 2: OUT per l'impostazione delle scelte diverse, dove la legge vuole il regime (60 km). Scelte diverse tra i
// tipi di servizio: K-CTR-132 variante 4 (dato particolare); tra i servizi: variante 3 (comune).
TEST(Orari, out_dalle_scelte_diverse_tipi_particolare_servizi_comune) {
    for (const bool tipi : {true, false}) {
        Prova561 a;
        alfa(a.p.p).trip_modes = tipi ? modi({per_tipo(ServiceKind::School, TripModeKind::Out), per_tipo(ServiceKind::Disability, TripModeKind::Tachograph)})
                                      : modi({}, {per_servizio("S-ASILO", TripModeKind::Out), per_servizio("S-DIS", TripModeKind::Tachograph)});
        alfa(a.p.p).trip_modes->mixed = MixedModes::Out;
        a.lunga("C-MISTA", {"S-ASILO", "S-DIS"}, 60.0);
        const auto v = a.p.problemi(Check::ModeOutNotValid);
        ASSERT_EQ(v.size(), 1u) << elenco(a.p.tutti());
        EXPECT_EQ(v[0].variant, tipi ? 4 : 3);
    }
}

// Scolastico e scolastico + disabili fino a 50 km esclusi con certezza (consulente, 28/10); solo disabili da confermare.
TEST(Orari, scolastico_escluso_solo_disabili_da_confermare) {
    for (const auto& [servizi, da_confermare] :
         std::vector<std::pair<std::vector<std::string>, bool>>{{{"S-ASILO"}, false}, {{"S-ASILO", "S-DIS"}, false}, {{"S-DIS"}, true}}) {
        Prova561 a;
        a.lunga("C-X", servizi, 30.0);
        EXPECT_TRUE(a.p.problemi(Check::LawDrivingViolated).empty()) << servizi.size();
        EXPECT_EQ(a.p.problemi(Check::LawDrivingToConfirm).size(), da_confermare ? 1u : 0u) << elenco(a.p.problemi(Check::LawDrivingToConfirm));
    }
}

// Dichiarazione dell'azienda su un servizio solo per disabili: escluso con certezza, una nota, nessun avviso da confermare;
// senza la dichiarazione gli avvisi restano.
TEST(Orari, dichiarazione_su_un_servizio_solo_disabili) {
    Prova561 a;
    a.lunga("C-DIS-X", {"S-DIS"}, 30.0);
    EXPECT_EQ(a.p.problemi(Check::LawDrivingToConfirm).size(), 1u);
    ModeEntry d = per_servizio("S-DIS", TripModeKind::Out);
    d.declaration = giustificazione("/aziende/0/modo_corse/servizi/0/dichiarazione");
    alfa(a.p.p).trip_modes = modi({}, {d});
    EXPECT_TRUE(a.p.problemi(Check::LawDrivingToConfirm).empty()) << elenco(a.p.problemi(Check::LawDrivingToConfirm));
    const auto n = a.p.problemi(Check::ModeDeclared);
    ASSERT_EQ(n.size(), 1u);
    EXPECT_EQ(parametro(n[0], "motivo")->sensitivity, Param::Sensitivity::Personal);
    EXPECT_EQ(n[0].where.pointer, "/aziende/0/modo_corse/servizi/0/dichiarazione");
}

// "out" dove la legge, con i dati scritti, vuole il 561 (una linea di 60 km): senza forzatura la legge vince (un avviso, e
// la violazione del 561 resta un errore); con la forzatura il programma pianifica out (nessun errore del 561), ma la
// forzatura resta visibile e il rischio dice le violazioni del 561 come avvisi.
TEST(Orari, out_dove_la_legge_vuole_il_561_senza_e_con_forzatura) {
    Prova561 a;
    alfa(a.p.p).trip_modes = modi({per_tipo(ServiceKind::Line, TripModeKind::Out)});
    a.lunga("C-60", {"S-LIN-B610"}, 60.0);
    EXPECT_EQ(a.p.problemi(Check::ModeOutNotValid).size(), 1u) << elenco(a.p.tutti());
    EXPECT_EQ(a.p.problemi(Check::LawDrivingViolated).size(), 1u);
    ModeEntry f = per_corsa("C-60", TripModeKind::Out);
    f.forcing = giustificazione("/aziende/0/modo_corse/corse/0/forzatura");
    alfa(a.p.p).trip_modes->by_trip = {f};
    EXPECT_TRUE(a.p.problemi(Check::ModeOutNotValid).empty()) << elenco(a.p.problemi(Check::ModeOutNotValid));
    EXPECT_TRUE(a.p.problemi(Check::LawDrivingViolated).empty()) << elenco(a.p.problemi(Check::LawDrivingViolated));
    const auto rischio = a.p.problemi(Check::LawDrivingToConfirm);
    ASSERT_EQ(rischio.size(), 1u) << elenco(rischio);
    EXPECT_TRUE(con_motivo(rischio[0], "forzatura_dell_azienda")) << elenco(rischio);
    ASSERT_EQ(a.p.problemi(Check::ModeForced).size(), 1u);
    const auto r = a.p.problemi(Check::ModeForcedRisk);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(parametro(r[0], "violazioni")->number, 1);
    EXPECT_EQ(r[0].variant, 2);   // sulla corsa, una violazione
    // Una forzatura scaduta (fino al giorno prima) non vale piu': la legge vince di nuovo.
    alfa(a.p.p).trip_modes->by_trip[0].forcing->until = giorno("2026-10-04");
    EXPECT_EQ(a.p.problemi(Check::LawDrivingViolated).size(), 1u);
}

// Il tachigrafo su una linea fino a 50 km (che la legge esclude): le regole del 561 si controllano come scelta dell'azienda.
// Preferenza: avviso; limite su un turno ricorrente: errore; limite su un'assegnazione del singolo giorno: avviso. La guida
// della linea e' guida.
TEST(Orari, tachigrafo_su_una_linea_fino_a_50_km) {
    const auto prova = [](ChoiceStrength forza, bool ricorrente, bool tipo = true) {
        Prova561 a;
        ModeEntry t = tipo ? per_tipo(ServiceKind::Line, TripModeKind::Tachograph) : per_servizio("S-LIN-B610", TripModeKind::Tachograph);
        t.strength = forza;
        alfa(a.p.p).trip_modes = tipo ? modi({t}) : modi({}, {t});
        a.lunga("C-10", {"S-LIN-B610"}, 10.0, ricorrente);
        EXPECT_TRUE(a.p.problemi(Check::LawDrivingViolated).empty());
        EXPECT_TRUE(a.p.problemi(Check::LawDrivingToConfirm).empty()) << elenco(a.p.problemi(Check::LawDrivingToConfirm));
        return std::make_pair(a.p.problemi(Check::ChosenRegimeLimit), a.p.problemi(Check::ChosenRegimeNotRespected));
    };
    const auto pref = prova(ChoiceStrength::Preference, true);
    EXPECT_TRUE(pref.first.empty());
    ASSERT_EQ(pref.second.size(), 1u);
    // Guida di fila (variante 0 della guida, 13); +30: la scelta e' per tipo di servizio, il problema rivela un dato particolare
    // (K-011 aggiunta 2). Con la scelta per il servizio, la variante comune.
    EXPECT_EQ(pref.second[0].variant, 13 + 30);
    EXPECT_TRUE(con_motivo(pref.second[0], "preferenza"));
    const auto servizio = prova(ChoiceStrength::Preference, true, false);
    ASSERT_EQ(servizio.second.size(), 1u);
    EXPECT_EQ(servizio.second[0].variant, 13);
    const auto lim = prova(ChoiceStrength::Limit, true);
    ASSERT_EQ(lim.first.size(), 1u);
    EXPECT_TRUE(lim.second.empty());
    const auto giorno_singolo = prova(ChoiceStrength::Limit, false);
    EXPECT_TRUE(giorno_singolo.first.empty());
    ASSERT_EQ(giorno_singolo.second.size(), 1u);
    EXPECT_TRUE(con_motivo(giorno_singolo.second[0], "assegnazione_del_giorno"));
}
