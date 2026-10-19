// K-Zero - prove del nucleo sui turni del giorno (K-008): chi fa ogni turno in una data, con quali corse e attivita',
// con quale mezzo e da quale dei tre modi viene l'assegnazione; i controlli sui turni (K-CTR-054 ... K-CTR-087).
// Si parte dal pacchetto demo e si aggiungono turni, rotazioni e assegnazioni scritti qui: le rotazioni di prova
// (3 giorni, 6+2, 14 giorni, mensile...) non possono stare tutte nel demo.
#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "controlli.hpp"
#include "controlli_catalogo.hpp"
#include "pacchetto_demo.hpp"
#include "turni.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

// --- Costruzione dei dati di prova (con le posizioni come le scriverebbe il lettore) ------------------------------

Source dove(const std::string& file, const std::string& puntatore) { return Source{file, puntatore, 0, 0}; }

Weekdays giorni_settimana(const std::vector<Weekday>& v) {
    Weekdays w;
    for (Weekday d : v) w.add(d);
    return w;
}

// Calendario per tutto il periodo del pacchetto, nei giorni della settimana indicati.
void calendario(Package& p, const std::string& codice, const std::vector<Weekday>& giorni,
                std::vector<CalendarLink> aggiunge = {}, std::vector<CalendarLink> toglie = {}) {
    Calendar c;
    c.code = codice;
    c.range = p.manifest.period;
    c.days = giorni_settimana(giorni);
    c.adds = std::move(aggiunge);
    c.removes = std::move(toglie);
    c.src = dove("calendari.json", "/calendari/" + std::to_string(p.calendars.size()));
    p.calendars.push_back(c);
}

const std::vector<Weekday> kTutti = {Weekday::Monday, Weekday::Tuesday, Weekday::Wednesday, Weekday::Thursday,
                                     Weekday::Friday, Weekday::Saturday, Weekday::Sunday};
const std::vector<Weekday> kLV = {Weekday::Monday, Weekday::Tuesday, Weekday::Wednesday, Weekday::Thursday, Weekday::Friday};

ShiftItem attivita(const std::string& tipo, const std::string& luogo, const std::string& dalle, const std::string& alle) {
    ShiftItem it;
    it.is_trip = false;
    it.activity.type = Ref{tipo, {}};
    it.activity.place = Ref{luogo, {}};
    it.activity.start = ora(dalle);
    it.activity.end = ora(alle);
    return it;
}

ShiftItem corsa(const std::string& codice) {
    ShiftItem it;
    it.is_trip = true;
    it.trip.trip = Ref{codice, {}};
    return it;
}

// Pezzo di corsa da un luogo a un altro (vuoto = dall'inizio o fino alla fine).
ShiftItem pezzo(const std::string& codice, const std::string& da, const std::string& a) {
    ShiftItem it = corsa(codice);
    if (!da.empty()) it.trip.from = TripPoint{Ref{da, {}}, std::nullopt, {}};
    if (!a.empty()) it.trip.to = TripPoint{Ref{a, {}}, std::nullopt, {}};
    return it;
}

// Aggiunge un turno con le posizioni dei dati (/turni/i, /turni/i/elementi/k, /turni/i/mezzo).
Shift& turno(Package& p, const std::string& codice, const std::string& azienda, const std::string& cal, std::vector<ShiftItem> elementi,
             ShiftVehicleMode modo = ShiftVehicleMode::None, const std::string& mezzo = "", ShiftRole ruolo = ShiftRole::Driver) {
    Shift s;
    const std::string pnt = "/turni/" + std::to_string(p.shifts.size());
    s.code = codice;
    s.company = Ref{azienda, dove("turni.json", pnt + "/azienda")};
    s.role = ruolo;
    s.calendar = Ref{cal, dove("turni.json", pnt + "/calendario")};
    s.vehicle_mode = modo;
    if (!mezzo.empty()) s.vehicle = Ref{mezzo, dove("turni.json", pnt + "/mezzo/codice")};
    s.vehicle_src = dove("turni.json", pnt + "/mezzo");
    const std::string rimessa = azienda == "BETA" ? "L-RIMESSA-BETA" : "L-RIMESSA-ALFA";
    s.start_place = Ref{rimessa, dove("turni.json", pnt + "/inizio")};
    s.end_place = Ref{rimessa, dove("turni.json", pnt + "/fine")};
    for (std::size_t k = 0; k < elementi.size(); ++k) {
        elementi[k].src = dove("turni.json", pnt + "/elementi/" + std::to_string(k));
        s.items.push_back(elementi[k]);
    }
    s.src = dove("turni.json", pnt);
    p.shifts.push_back(s);
    return p.shifts.back();
}

// Turno di sola attivita' (riserva in rimessa) dalle alle: comodo per provare le rotazioni.
Shift& turno_semplice(Package& p, const std::string& codice, const std::string& cal, const std::string& dalle, const std::string& alle,
                      const std::string& azienda = "ALFA") {
    return turno(p, codice, azienda, cal, {attivita(azienda == "BETA" ? "SOSTA" : "RISERVA", azienda == "BETA" ? "L-RIMESSA-BETA" : "L-RIMESSA-ALFA", dalle, alle)});
}

// Riga di una rotazione: un passo per giorno (o mese); "R" = riposo, altrimenti i codici dei turni separati da "+".
struct Riga {
    std::string persona;
    std::vector<std::string> passi;
};

Rotation& rotazione(Package& p, const std::string& codice, int lunghezza, const std::string& partenza, const std::vector<Riga>& righe,
                    RotationUnit passo = RotationUnit::Day) {
    Rotation r;
    const std::string pnt = "/rotazioni/" + std::to_string(p.rotations.size());
    r.code = codice;
    r.unit = passo;
    r.length = lunghezza;
    r.start = giorno(partenza);
    for (std::size_t i = 0; i < righe.size(); ++i) {
        RotationRow row;
        const std::string pr = pnt + "/righe/" + std::to_string(i);
        row.person = Ref{righe[i].persona, dove("rotazioni.json", pr + "/persona")};
        for (std::size_t k = 0; k < righe[i].passi.size(); ++k) {
            RotationStep st;
            st.src = dove("rotazioni.json", pr + "/passi/" + std::to_string(k));
            const std::string& testo = righe[i].passi[k];
            if (testo == "R") {
                st.rest = true;
            } else {
                std::size_t inizio = 0;
                while (inizio <= testo.size()) {
                    const std::size_t fine = std::min(testo.find('+', inizio), testo.size());
                    st.shifts.push_back(Ref{testo.substr(inizio, fine - inizio), {}});
                    inizio = fine + 1;
                }
            }
            row.steps.push_back(st);
        }
        row.src = dove("rotazioni.json", pr);
        r.rows.push_back(row);
    }
    r.src = dove("rotazioni.json", pnt);
    p.rotations.push_back(r);
    return p.rotations.back();
}

FixedAssignment& fisso(Package& p, const std::string& codice, const std::string& turno, const std::string& persona, const std::string& cal) {
    const std::string pnt = "/servizi_fissi/" + std::to_string(p.fixed.size());
    p.fixed.push_back({codice, Ref{turno, {}}, Ref{persona, {}}, Ref{cal, {}}, "", dove("servizi_fissi.json", pnt)});
    return p.fixed.back();
}

DayAssignment& del_giorno(Package& p, const std::string& data, const std::string& turno, const std::string& persona,
                          const std::string& mezzo = "") {
    const std::string pnt = "/assegnazioni/" + std::to_string(p.day_assignments.size());
    DayAssignment a;
    a.date = giorno(data);
    a.shift = Ref{turno, {}};
    if (!persona.empty()) a.person = Ref{persona, {}};
    if (!mezzo.empty()) a.vehicle = Ref{mezzo, {}};
    a.src = dove("assegnazioni.json", pnt);
    p.day_assignments.push_back(a);
    return p.day_assignments.back();
}

// --- Lettura del piano --------------------------------------------------------------------------------------------

const ShiftDay* turno_del_giorno(const std::vector<ShiftDay>& piano, const std::string& codice) {
    for (const ShiftDay& sd : piano)
        if (sd.shift->code == codice) return &sd;
    return nullptr;
}
// Un turno di un elenco temporaneo (es. turno_del_giorno(piano.day(...), ...)) sparirebbe con l'elenco: vietato.
const ShiftDay* turno_del_giorno(std::vector<ShiftDay>&& piano, const std::string& codice) = delete;

// Chi fa il turno quel giorno: il codice della persona, "-" se scoperto, "assente" se il turno quel giorno non c'e'.
std::string chi(const Package& p, const std::string& turno, const std::string& data) {
    const Planner piano(p);
    const auto giornata = piano.day(giorno(data));
    const ShiftDay* sd = turno_del_giorno(giornata, turno);
    if (!sd) return "assente";
    return sd->person ? sd->person->code : "-";
}

// Turni di una persona in un giorno, in ordine di inizio, separati da "+" ("riposo" se nessuno).
std::string turni_di(const Planner& piano, const std::string& persona, const std::string& data) {
    std::string s;
    for (const ShiftDay& sd : piano.day(giorno(data)))
        if (sd.person && sd.person->code == persona) s += (s.empty() ? "" : "+") + sd.shift->code;
    return s.empty() ? "riposo" : s;
}

// --- Controlli ---------------------------------------------------------------------------------------------------

std::string elenco(const std::vector<Problem>& v) {
    std::string s;
    for (const Problem& x : v)
        s += "\n  " + api::definizione_controllo(x.check).codice + " variante " + std::to_string(x.variant) + " in " + x.where.file + " " +
             x.where.pointer;
    return s.empty() ? " nessuno" : s;
}

const Problem* trova(const std::vector<Problem>& v, Check c, int variante, const std::string& file, const std::string& puntatore) {
    for (const Problem& x : v)
        if (x.check == c && x.variant == variante && x.where.file == file && x.where.pointer == puntatore) return &x;
    return nullptr;
}

::testing::AssertionResult segnala(const Package& p, Check c, int variante, const std::string& file, const std::string& puntatore) {
    const auto v = check_package(p);
    if (trova(v, c, variante, file, puntatore)) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << api::definizione_controllo(c).codice << " variante " << variante << " in " << file << " "
                                         << puntatore << " non segnalato. Problemi trovati:" << elenco(v);
}

int quanti(const Package& p, Check c, int variante = -1) {
    const auto v = check_package(p);
    return static_cast<int>(
        std::count_if(v.begin(), v.end(), [&](const Problem& x) { return x.check == c && (variante < 0 || x.variant == variante); }));
}

// Giorni di un problema (parametro Days).
std::vector<Day> giorni_di(const Problem& x) {
    for (const Param& p : x.params)
        if (p.kind == Param::Kind::Days) return p.days;
    return {};
}

// Pacchetto di prova: il demo senza i suoi turni (ogni prova scrive i suoi), con un calendario di tutti i giorni
// (CAL-TUTTI), uno da lunedi' a venerdi' (CAL-LV), uno del sabato (CAL-SAB) e uno del mercoledi' (CAL-MER).
Package base() {
    Package p = demo();
    p.shifts.clear();
    p.rotations.clear();
    p.fixed.clear();
    p.day_assignments.clear();
    p.blocks.clear();
    calendario(p, "CAL-TUTTI", kTutti);
    calendario(p, "CAL-LV", kLV);
    calendario(p, "CAL-SAB", {Weekday::Saturday});
    calendario(p, "CAL-MER", {Weekday::Wednesday});
    return p;
}

}  // namespace

// --- Il pacchetto demo: giorni controllati a mano ----------------------------------------------------------------

// Con quale mezzo si fa il turno quel giorno: il codice del mezzo, "-" se senza mezzo, "assente" se il turno non c'e'.
std::string mezzo_di(const Package& p, const std::string& turno, const std::string& data) {
    const Planner piano(p);
    const auto giornata = piano.day(giorno(data));
    const ShiftDay* sd = turno_del_giorno(giornata, turno);
    if (!sd) return "assente";
    return sd->vehicle ? sd->vehicle->code : "-";
}

// Rotazione di Alfa (14 giorni dal lunedi' 2026-08-31, la tabella di REGOLE.md), rotazione di Beta (3 giorni dal
// 2026-09-01), servizi fissi e assegnazioni del singolo giorno del demo, giorno per giorno.
TEST(Turni, demo_giorni_controllati_a_mano) {
    const Package& p = demo();
    // Giovedi' 2026-10-15, giorno 4 del ciclo: P-A primo, P-B secondo (mattino al centro diurno, sera sulla linea).
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-10-15"), "P-A");
    EXPECT_EQ(chi(p, "T-DIS", "2026-10-15"), "P-B");
    EXPECT_EQ(chi(p, "T-SECONDO", "2026-10-15"), "P-B");
    EXPECT_EQ(chi(p, "T-DIS-GIO", "2026-10-15"), "P-E");
    EXPECT_EQ(chi(p, "T-NOTTE", "2026-10-15"), "P-NUOVO");   // autista ipotetico
    EXPECT_EQ(chi(p, "T-RISERVA", "2026-10-15"), "assente");  // solo martedi' e venerdi'
    // Mercoledi' 2026-10-21, giorno 10: il secondo e' del titolare P-C (servizio fisso), il mattino resta a P-B.
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-10-21"), "P-A");
    EXPECT_EQ(chi(p, "T-DIS", "2026-10-21"), "P-B");
    EXPECT_EQ(chi(p, "T-SECONDO", "2026-10-21"), "P-C");
    // Giovedi' 2026-10-22, giorno 11: P-A (secondo) e' in ferie, lo sostituisce P-C con due assegnazioni del giorno.
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-10-22"), "P-B");
    EXPECT_EQ(chi(p, "T-DIS", "2026-10-22"), "P-C");
    EXPECT_EQ(chi(p, "T-SECONDO", "2026-10-22"), "P-C");
    // Sabato 2026-10-24, giorno 13: tutta la linea a P-A.
    EXPECT_EQ(chi(p, "T-SABATO", "2026-10-24"), "P-A");
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-10-24"), "assente");
    // Revisione di ALFA-01 (lunedi' e martedi'): la scuola delle 06:55 va con il mezzo di scorta.
    EXPECT_EQ(mezzo_di(p, "T-SCUOLA-1", "2026-10-19"), "ALFA-07");
    EXPECT_EQ(mezzo_di(p, "T-SCUOLA-1", "2026-10-21"), "ALFA-01");
    // Venerdi' 2026-11-20, giorno 12: P-B (primo) in formazione e nessuna sostituzione: il primo e' scoperto.
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-11-20"), "-");
    EXPECT_EQ(chi(p, "T-SECONDO", "2026-11-20"), "P-A");
    // Mercoledi' 2026-11-11, patrono di Valfonda: niente scuola, la linea si'.
    EXPECT_EQ(chi(p, "T-SCUOLA-1", "2026-11-11"), "assente");
    EXPECT_EQ(chi(p, "T-SECONDO", "2026-11-11"), "P-C");
    // Beta, martedi' 2026-11-03 (primo passo del ciclo): P-B2 in ferie, al suo posto P-C prestato da Alfa.
    EXPECT_EQ(chi(p, "T-B-PRIMO", "2026-11-03"), "P-B1");
    EXPECT_EQ(chi(p, "T-B-ASILO", "2026-11-03"), "P-C");
    EXPECT_EQ(chi(p, "T-B-SERA", "2026-11-03"), "P-C");
    EXPECT_EQ(chi(p, "T-B-ACCOMP", "2026-11-03"), "P-B4");
    // Domenica 2026-11-15: la gita con l'autobus di linea (P-A, giorno di riposo) e la linea festiva di Beta.
    EXPECT_EQ(chi(p, "T-GITA", "2026-11-15"), "P-A");
    EXPECT_EQ(mezzo_di(p, "T-GITA", "2026-11-15"), "ALFA-05");
    EXPECT_EQ(chi(p, "T-B-FESTIVO", "2026-11-15"), "P-B1");
    // Martedi' 2026-12-08, festa nazionale: orario festivo (terzo passo del ciclo di Beta: P-B3), Alfa ferma.
    EXPECT_EQ(chi(p, "T-B-FESTIVO", "2026-12-08"), "P-B3");
    EXPECT_EQ(chi(p, "T-B-PRIMO", "2026-12-08"), "assente");
    EXPECT_EQ(chi(p, "T-PRIMO", "2026-12-08"), "assente");
    EXPECT_EQ(chi(p, "T-NOTTE", "2026-12-08"), "assente");
    // Mercoledi' 2027-07-07: la CQC di P-C e' scaduta il 2027-06-30, il secondo del mercoledi' e' scoperto.
    EXPECT_EQ(chi(p, "T-SECONDO", "2027-07-07"), "-");
}

// --- Rotazioni di ogni forma: lunghezza e passi li sceglie l'azienda ------------------------------------------------

// Ciclo di 3 giorni con tre persone e il riposo che scorre; il ciclo si conta anche prima della partenza.
TEST(Turni, ciclo_di_3_giorni) {
    Package p = base();
    turno_semplice(p, "MATTINO", "CAL-TUTTI", "06:00", "13:00");
    turno_semplice(p, "SERA", "CAL-TUTTI", "13:00", "20:00");
    rotazione(p, "ROT-3", 3, "2026-10-12",
              {{"P-A", {"MATTINO", "SERA", "R"}}, {"P-B", {"SERA", "R", "MATTINO"}}, {"P-C", {"R", "MATTINO", "SERA"}}});
    p.rotations[0].from = giorno("2026-09-01");
    const Planner piano(p);
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-12"), "MATTINO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-12"), "SERA");
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-12"), "riposo");
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-13"), "SERA");
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-13"), "MATTINO");
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-14"), "riposo");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-14"), "MATTINO");
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-15"), "MATTINO");   // il ciclo riparte
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-11"), "riposo");    // il giorno prima della partenza = terzo passo
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-11"), "MATTINO");
    // Il turno sa da dove viene la persona: rotazione, riga e passo.
    const auto g = piano.day(giorno("2026-10-13"));
    const ShiftDay* sd = turno_del_giorno(g, "MATTINO");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->person_source, PersonSource::Rotation);
    EXPECT_EQ(sd->rotation.rotation->code, "ROT-3");
    EXPECT_EQ(sd->rotation.step, 1);
}

// Ciclo di 8 giorni (6 di lavoro e 2 di riposo) che parte di mercoledi' e attraversa le settimane.
TEST(Turni, ciclo_6_piu_2_attraverso_le_settimane) {
    Package p = base();
    turno_semplice(p, "LAVORO", "CAL-TUTTI", "07:00", "15:00");
    rotazione(p, "ROT-8", 8, "2026-10-14", {{"P-C", {"LAVORO", "LAVORO", "LAVORO", "LAVORO", "LAVORO", "LAVORO", "R", "R"}}});
    const Planner piano(p);
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-14"), "LAVORO");   // mercoledi', primo giorno
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-19"), "LAVORO");   // lunedi', sesto giorno
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-20"), "riposo");   // martedi'
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-21"), "riposo");   // mercoledi'
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-22"), "LAVORO");   // giovedi': il ciclo riparte, il riposo scorre
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-28"), "riposo");   // mercoledi' della settimana dopo
    EXPECT_EQ(turni_di(piano, "P-C", "2026-10-13"), "riposo");   // prima della partenza la rotazione non vale (senza dal)
}

// Rotazione di 14 giorni di Alpinabus (REGOLE.md, "Turni del giorno e rotazione"), controllata a mano contro la
// tabella: lunedi' 12/10/2026 = giorno 1; il sabato un solo autista fa tutto il giorno; la domenica nessuno.
TEST(Turni, rotazione_di_14_giorni_della_tabella) {
    Package p = base();
    turno_semplice(p, "PRIMO", "CAL-LV", "06:30", "17:00");
    turno_semplice(p, "SECONDO", "CAL-LV", "08:00", "20:30");
    turno(p, "SABATO", "ALFA", "CAL-SAB",
          {attivita("RISERVA", "L-RIMESSA-ALFA", "06:30", "19:00"), attivita("PULIZIA", "L-RIMESSA-ALFA", "19:00", "20:30")});
    rotazione(p, "ROT-14", 14, "2026-10-12",
              {{"P-A", {"PRIMO", "SECONDO", "SECONDO", "PRIMO", "PRIMO", "R", "R", "SECONDO", "PRIMO", "PRIMO", "SECONDO", "SECONDO", "SABATO", "R"}},
               {"P-B", {"SECONDO", "PRIMO", "PRIMO", "SECONDO", "SECONDO", "SABATO", "R", "PRIMO", "SECONDO", "SECONDO", "PRIMO", "PRIMO", "R", "R"}}});
    const Planner piano(p);
    // Giorno 1, lunedi': A primo, B secondo.
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-12"), "PRIMO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-12"), "SECONDO");
    // Giorno 6, sabato della settimana 1: A riposo, B tutto il giorno (con la pulizia).
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-17"), "riposo");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-17"), "SABATO");
    // Giorno 7, domenica: nessuno.
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-18"), "riposo");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-18"), "riposo");
    EXPECT_TRUE(piano.day(giorno("2026-10-18")).empty());
    // Giorno 10, mercoledi' della settimana 2: A primo, B secondo.
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-21"), "PRIMO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-21"), "SECONDO");
    // Giorno 13, sabato della settimana 2: A tutto il giorno, B riposo.
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-24"), "SABATO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-24"), "riposo");
    // Il giorno dopo la fine del primo ciclo (lunedi' 26/10) e' di nuovo il giorno 1.
    EXPECT_EQ(turni_di(piano, "P-A", "2026-10-26"), "PRIMO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-10-26"), "SECONDO");
    // Il ciclo continua anche nelle vacanze: lunedi' 28/12 e' il giorno 8 (A secondo, B primo).
    EXPECT_EQ(turni_di(piano, "P-A", "2026-12-28"), "SECONDO");
    EXPECT_EQ(turni_di(piano, "P-B", "2026-12-28"), "PRIMO");
    // Il turno del sabato comprende la pulizia: inizio e fine ricavati dalle attivita'.
    const auto sab = piano.day(giorno("2026-10-17"));
    ASSERT_EQ(sab.size(), 1u);
    EXPECT_EQ(sab[0].start, ora("06:30"));
    EXPECT_EQ(sab[0].end, ora("20:30"));
    EXPECT_EQ(sab[0].items.size(), 2u);
    // Nessun turno scoperto nel periodo coperto dalla rotazione (dal 12/10).
    for (const Problem& x : check_package(p)) {
        if (x.check != Check::ShiftUncovered) continue;
        for (Day d : giorni_di(x)) {
            EXPECT_LT(d, giorno("2026-10-12")) << text_from_day(d);
        }
    }
}

// Rotazione a passi mensili: un mese a testa.
TEST(Turni, rotazione_a_passi_mensili) {
    Package p = base();
    turno_semplice(p, "MESE", "CAL-TUTTI", "07:00", "12:00");
    rotazione(p, "ROT-MESI", 2, "2026-10-01", {{"P-A", {"MESE", "R"}}, {"P-B", {"R", "MESE"}}}, RotationUnit::Month);
    p.rotations[0].from = giorno("2026-09-01");
    EXPECT_EQ(chi(p, "MESE", "2026-10-01"), "P-A");
    EXPECT_EQ(chi(p, "MESE", "2026-10-31"), "P-A");
    EXPECT_EQ(chi(p, "MESE", "2026-11-01"), "P-B");
    EXPECT_EQ(chi(p, "MESE", "2026-12-15"), "P-A");
    EXPECT_EQ(chi(p, "MESE", "2027-01-20"), "P-B");
    EXPECT_EQ(chi(p, "MESE", "2026-09-10"), "P-B");   // settembre, prima della partenza: passo precedente
}

// Cambio di rotazione tra inverno ed estate (date di validita').
TEST(Turni, cambio_di_rotazione_inverno_estate) {
    Package p = base();
    turno_semplice(p, "T1", "CAL-TUTTI", "07:00", "12:00");
    Rotation& inverno = rotazione(p, "ROT-INVERNO", 1, "2026-09-01", {{"P-A", {"T1"}}});
    inverno.to = giorno("2027-06-12");
    Rotation& estate = rotazione(p, "ROT-ESTATE", 2, "2027-06-13", {{"P-B", {"T1", "R"}}, {"P-C", {"R", "T1"}}});
    estate.from = giorno("2027-06-13");
    EXPECT_EQ(chi(p, "T1", "2027-06-12"), "P-A");
    EXPECT_EQ(chi(p, "T1", "2027-06-13"), "P-B");
    EXPECT_EQ(chi(p, "T1", "2027-06-14"), "P-C");
    EXPECT_EQ(quanti(p, Check::RotationTwoPersons), 0);   // le due rotazioni non si toccano
    // Se si toccano, nello stesso giorno due persone hanno lo stesso turno: errore.
    estate.from = giorno("2027-06-12");
    EXPECT_TRUE(segnala(p, Check::RotationTwoPersons, 0, "rotazioni.json", "/rotazioni/1/righe/1"));   // P-C, il 12/06
}

// Due rotazioni contemporanee per gruppi diversi di persone (anche di aziende diverse).
TEST(Turni, due_rotazioni_contemporanee) {
    Package p = base();
    turno_semplice(p, "A1", "CAL-TUTTI", "07:00", "12:00");
    turno_semplice(p, "B1", "CAL-TUTTI", "07:00", "12:00", "BETA");
    rotazione(p, "ROT-ALFA", 2, "2026-10-12", {{"P-A", {"A1", "R"}}, {"P-B", {"R", "A1"}}});
    rotazione(p, "ROT-BETA", 3, "2026-10-12", {{"P-B1", {"B1", "R", "R"}}, {"P-B2", {"R", "B1", "R"}}, {"P-B3", {"R", "R", "B1"}}});
    EXPECT_EQ(chi(p, "A1", "2026-10-13"), "P-B");
    EXPECT_EQ(chi(p, "B1", "2026-10-13"), "P-B2");
    EXPECT_EQ(chi(p, "A1", "2026-10-14"), "P-A");
    EXPECT_EQ(chi(p, "B1", "2026-10-14"), "P-B3");
}

// Azienda senza rotazione: i turni li dicono solo servizi fissi e assegnazioni del giorno; altrimenti scoperti.
TEST(Turni, azienda_senza_rotazione) {
    Package p = base();
    turno_semplice(p, "B1", "CAL-LV", "07:00", "12:00", "BETA");
    fisso(p, "SF-B1", "B1", "P-B1", "CAL-LV");
    turno_semplice(p, "B2", "CAL-LV", "13:00", "18:00", "BETA");
    del_giorno(p, "2026-10-15", "B2", "P-B2");
    EXPECT_EQ(chi(p, "B1", "2026-10-15"), "P-B1");
    EXPECT_EQ(chi(p, "B2", "2026-10-15"), "P-B2");
    EXPECT_EQ(chi(p, "B2", "2026-10-16"), "-");   // nessuno lo assegna: scoperto, il programma non inventa
    EXPECT_TRUE(segnala(p, Check::ShiftUncovered, 0, "turni.json", "/turni/1"));
}

// Servizio fisso solo il mercoledi': quel giorno vince sulla rotazione, gli altri giorni segue la rotazione.
TEST(Turni, servizio_fisso_solo_il_mercoledi) {
    Package p = base();
    turno_semplice(p, "MEZZOGIORNO", "CAL-LV", "11:30", "13:30");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-A", {"MEZZOGIORNO"}}});
    fisso(p, "SF-MER", "MEZZOGIORNO", "P-C", "CAL-MER");
    EXPECT_EQ(chi(p, "MEZZOGIORNO", "2026-10-13"), "P-A");   // martedi'
    EXPECT_EQ(chi(p, "MEZZOGIORNO", "2026-10-14"), "P-C");   // mercoledi'
    EXPECT_EQ(chi(p, "MEZZOGIORNO", "2026-10-15"), "P-A");   // giovedi'
    const Planner piano(p);
    const auto g = piano.day(giorno("2026-10-14"));
    EXPECT_EQ(turno_del_giorno(g, "MEZZOGIORNO")->person_source, PersonSource::Fixed);
    EXPECT_EQ(turno_del_giorno(g, "MEZZOGIORNO")->fixed->code, "SF-MER");
}

// Sostituzione di un giorno: l'assegnazione del singolo giorno vince su servizio fisso e rotazione.
TEST(Turni, sostituzione_di_un_giorno) {
    Package p = base();
    turno_semplice(p, "PRIMO", "CAL-LV", "06:30", "12:00");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-A", {"PRIMO"}}});
    fisso(p, "SF", "PRIMO", "P-B", "CAL-MER");
    del_giorno(p, "2026-10-14", "PRIMO", "P-C");   // mercoledi': vince anche sul servizio fisso
    del_giorno(p, "2026-10-15", "PRIMO", "P-C");
    EXPECT_EQ(chi(p, "PRIMO", "2026-10-14"), "P-C");
    EXPECT_EQ(chi(p, "PRIMO", "2026-10-15"), "P-C");
    EXPECT_EQ(chi(p, "PRIMO", "2026-10-16"), "P-A");
    EXPECT_EQ(chi(p, "PRIMO", "2026-10-21"), "P-B");
    const Planner piano(p);
    const auto giovedi = piano.day(giorno("2026-10-15"));
    EXPECT_EQ(turno_del_giorno(giovedi, "PRIMO")->person_source, PersonSource::Day);
}

// Corse di calendari diversi nello stesso turno: in un giorno di vacanza scolastica restano solo quelle di linea.
TEST(Turni, corse_di_calendari_diversi_in_vacanza) {
    Package p = base();
    turno(p, "MISTO", "ALFA", "CAL-FERIALI", {corsa("C-AM-1"), corsa("C-LIN-533")}, ShiftVehicleMode::Specific, "ALFA-X30");
    const Planner piano(p);
    const auto scuola = piano.day(giorno("2026-10-15"));   // giovedi' di scuola
    const ShiftDay* a = turno_del_giorno(scuola, "MISTO");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->items.size(), 2u);
    EXPECT_EQ(a->start, ora("06:55"));
    EXPECT_EQ(a->end, ora("08:15"));
    const auto vacanza = piano.day(giorno("2026-12-28"));   // lunedi' delle vacanze di Natale, feriale
    const ShiftDay* b = turno_del_giorno(vacanza, "MISTO");
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(b->items.size(), 1u);
    EXPECT_EQ(b->items[0].trip->code, "C-LIN-533");
    EXPECT_EQ(b->start, ora("07:52"));   // inizio e fine ricavati dalle corse di quel giorno
    EXPECT_EQ(b->end, ora("08:15"));
    const auto immacolata = piano.day(giorno("2026-12-08"));
    EXPECT_EQ(turno_del_giorno(immacolata, "MISTO"), nullptr);   // festivita': niente
}

// Turno festivo in una festivita' infrasettimanale: domeniche piu' festivita', scritte una volta sola.
TEST(Turni, turno_festivo_in_una_festivita_infrasettimanale) {
    Package p = base();
    calendario(p, "CAL-FESTIVI", {Weekday::Sunday}, {{CalendarLinkKind::Holidays, Ref{"FEST-IT", {}}}});
    turno_semplice(p, "FESTIVO", "CAL-FESTIVI", "08:00", "13:00", "BETA");
    turno_semplice(p, "FERIALE", "CAL-FERIALI", "08:00", "13:00", "BETA");
    rotazione(p, "ROT-B", 1, "2026-09-01", {{"P-B1", {"FESTIVO+FERIALE"}}});   // i due turni non sono mai nello stesso giorno
    const Planner piano(p);
    EXPECT_EQ(turni_di(piano, "P-B1", "2026-12-08"), "FESTIVO");   // martedi', Immacolata
    EXPECT_EQ(turni_di(piano, "P-B1", "2026-12-07"), "FERIALE");   // lunedi'
    EXPECT_EQ(turni_di(piano, "P-B1", "2026-12-13"), "FESTIVO");   // domenica
    EXPECT_EQ(turni_di(piano, "P-B1", "2027-03-29"), "FESTIVO");   // lunedi' dell'Angelo (festa mobile, scritta come data)
}

// --- Principio: un fatto con le date su un turno ricorrente = avviso e turno scoperto o senza mezzo; su
// un'assegnazione del singolo giorno = errore; contro un'impostazione o una preferenza = avviso ------------------

TEST(Turni, assenza_su_un_turno_della_rotazione_avviso_e_scoperto) {
    Package p = base();
    turno_semplice(p, "PRIMO", "CAL-LV", "06:30", "12:00");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-A", {"PRIMO"}}});   // P-A in ferie il 22 e 23/10 (assenze.json)
    const Planner piano(p);
    const auto giornata_sd = piano.day(giorno("2026-10-22"));
    const ShiftDay* sd = turno_del_giorno(giornata_sd, "PRIMO");
    ASSERT_NE(sd, nullptr);
    EXPECT_TRUE(sd->uncovered());
    EXPECT_EQ(sd->planned->code, "P-A");
    EXPECT_EQ(sd->person_impediment, PersonImpediment::Absent);
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::RecurringShiftPersonFact, 0, "assenze.json", "/assenze/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    EXPECT_EQ(api::definizione_controllo(x->check).livello, api::Livello::Avviso);
    EXPECT_EQ(giorni_di(*x), (std::vector<Day>{giorno("2026-10-22"), giorno("2026-10-23")}));   // un avviso per i due giorni
    EXPECT_EQ(quanti(p, Check::DayAssignmentAgainstFact), 0);
    // Il rimedio: un'assegnazione del singolo giorno. L'avviso resta solo per il giorno non coperto.
    del_giorno(p, "2026-10-22", "PRIMO", "P-C");
    const auto w = check_package(p);
    const Problem* y = trova(w, Check::RecurringShiftPersonFact, 0, "assenze.json", "/assenze/0");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(giorni_di(*y), (std::vector<Day>{giorno("2026-10-23")}));
}

TEST(Turni, assenza_di_una_parte_del_giorno_e_a_cavallo_della_mezzanotte) {
    Package p = base();
    turno_semplice(p, "NOTTE", "CAL-TUTTI", "22:00", "25:00");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-B", {"NOTTE"}}});
    p.absences.push_back({Ref{"P-B", {}}, Ref{"FERIE", {}}, {giorno("2026-11-03"), giorno("2026-11-03")}, ora("00:00"), ora("02:00"),
                          dove("assenze.json", "/assenze/1")});
    EXPECT_EQ(chi(p, "NOTTE", "2026-11-02"), "-");     // il turno del 2 finisce alle 01:00 del 3: tocca l'assenza
    EXPECT_EQ(chi(p, "NOTTE", "2026-11-03"), "P-B");   // il turno del 3 comincia alle 22:00: l'assenza e' finita
}

TEST(Turni, disponibilita_rigida) {
    Package p = base();
    turno_semplice(p, "POMERIGGIO", "CAL-LV", "13:00", "17:00");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-D", {"POMERIGGIO"}}});   // P-D disponibile da lunedi' a venerdi' 06:30-14:00
    const Planner piano(p);
    const auto giornata_sd = piano.day(giorno("2026-10-15"));
    const ShiftDay* sd = turno_del_giorno(giornata_sd, "POMERIGGIO");
    ASSERT_NE(sd, nullptr);
    EXPECT_TRUE(sd->uncovered());
    EXPECT_EQ(sd->person_impediment, PersonImpediment::OutsideAvailability);
    EXPECT_TRUE(segnala(p, Check::RecurringShiftPersonFact, 1, "persone.json", "/persone/3/disponibilita"));
}

TEST(Turni, mezzo_in_fermo_su_un_turno_ricorrente_avviso_e_senza_mezzo) {
    Package p = base();
    turno(p, "SCUOLA", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-B", {"SCUOLA"}}});
    const Planner piano(p);
    const auto giornata_sd = piano.day(giorno("2026-10-19"));   // ALFA-01 in revisione 19-20/10
    const ShiftDay* sd = turno_del_giorno(giornata_sd, "SCUOLA");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->vehicle, nullptr);
    EXPECT_EQ(sd->planned_vehicle->code, "ALFA-01");
    EXPECT_EQ(sd->vehicle_impediment, VehicleImpediment::Stopped);
    EXPECT_EQ(sd->person->code, "P-B");   // la persona c'e'
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::RecurringShiftVehicleFact, 0, "fermi.json", "/fermi/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    EXPECT_EQ(giorni_di(*x).size(), 2u);
    EXPECT_EQ(api::definizione_controllo(x->check).livello, api::Livello::Avviso);
    // Rimedio: un altro mezzo con l'assegnazione del singolo giorno.
    del_giorno(p, "2026-10-19", "SCUOLA", "", "ALFA-02");
    del_giorno(p, "2026-10-20", "SCUOLA", "", "ALFA-02");
    EXPECT_EQ(quanti(p, Check::RecurringShiftVehicleFact), 0);
}

TEST(Turni, assegnazione_del_giorno_su_una_persona_assente_errore) {
    Package p = base();
    turno_semplice(p, "PRIMO", "CAL-LV", "06:30", "12:00");
    del_giorno(p, "2026-10-22", "PRIMO", "P-A");   // P-A e' in ferie
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::DayAssignmentAgainstFact, 0, "assegnazioni.json", "/assegnazioni/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    EXPECT_EQ(api::definizione_controllo(x->check).livello, api::Livello::Errore);
    // Mezzo in fermo messo con un'assegnazione del giorno: errore.
    Package q = base();
    turno(q, "SCUOLA", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-02");
    del_giorno(q, "2026-10-19", "SCUOLA", "P-B", "ALFA-01");
    EXPECT_TRUE(segnala(q, Check::DayAssignmentAgainstFact, 5, "assegnazioni.json", "/assegnazioni/0"));
}

TEST(Turni, assegnazione_contro_l_abbinamento_fisso_avviso) {
    Package p = base();
    p.company_settings[0].pairing = DriverVehiclePairing::Fixed;
    per_codice(p.persons, "P-A").usual_vehicle = Ref{"ALFA-01", {}};
    per_codice(p.persons, "P-B").usual_vehicle = Ref{"ALFA-02", {}};
    turno(p, "SCUOLA", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-A", {"SCUOLA"}}});   // ALFA-01 e' il mezzo abituale di P-A
    del_giorno(p, "2026-10-15", "SCUOLA", "P-B");                  // P-B ha ALFA-02: la scelta dell'utente vince
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::PairingNotRespected, 2, "assegnazioni.json", "/assegnazioni/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    EXPECT_EQ(api::definizione_controllo(x->check).livello, api::Livello::Avviso);
    EXPECT_EQ(quanti(p, Check::PairingFixedViolated), 0);
    // Un turno ricorrente contro l'abbinamento fisso e' sempre sbagliato: errore.
    rotazione(p, "ROT-2", 1, "2026-10-12", {{"P-B", {"SCUOLA-2"}}});
    turno(p, "SCUOLA-2", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-2")}, ShiftVehicleMode::Specific, "ALFA-03");
    EXPECT_TRUE(segnala(p, Check::PairingFixedViolated, 0, "rotazioni.json", "/rotazioni/1/righe/0"));
}

// --- Pezzi di corsa: cambio dell'autista lungo la corsa ---------------------------------------------------------

TEST(Turni, cambio_dell_autista_a_meta_corsa) {
    Package p = base();
    turno(p, "PEZZO-1", "ALFA", "CAL-FERIALI", {pezzo("C-LIN-533", "", "F-CASTELROVO-ALTO")}, ShiftVehicleMode::Specific, "ALFA-X30");
    turno(p, "PEZZO-2", "ALFA", "CAL-FERIALI", {pezzo("C-LIN-533", "F-CASTELROVO-ALTO", "")}, ShiftVehicleMode::Specific, "ALFA-X30");
    rotazione(p, "ROT", 1, "2026-09-01", {{"P-A", {"PEZZO-1"}}, {"P-B", {"PEZZO-2"}}});
    const Planner piano(p);
    const auto g = piano.day(giorno("2026-10-15"));
    const ShiftDay* a = turno_del_giorno(g, "PEZZO-1");
    const ShiftDay* b = turno_del_giorno(g, "PEZZO-2");
    ASSERT_TRUE(a && b);
    EXPECT_FALSE(a->items[0].whole);
    EXPECT_EQ(a->end, ora("08:08"));
    EXPECT_EQ(b->start, ora("08:08"));
    EXPECT_DOUBLE_EQ(a->items[0].loaded_km + b->items[0].loaded_km, 15.1);   // i km dei pezzi fanno la corsa
    // I pezzi combaciano: nessun buco, nessuna sovrapposizione; lo stesso mezzo non si sovrappone.
    EXPECT_EQ(quanti(p, Check::TripPartsGap), 0);
    EXPECT_EQ(quanti(p, Check::TripInTwoShifts), 0);
    EXPECT_EQ(quanti(p, Check::VehicleOverlap), 0);
    // Buco: il primo pezzo finisce una fermata prima di dove parte il secondo.
    p.shifts[0].items[0].trip.to = TripPoint{Ref{"F-VALFONDA-PIAZZA", {}}, std::nullopt, {}};
    p.shifts[1].items[0].trip.from = TripPoint{Ref{"F-CASTELROVO-ALTO", {}}, std::nullopt, {}};
    EXPECT_TRUE(segnala(p, Check::TripPartsGap, 0, "turni.json", "/turni/1/elementi/0"));
    // Sovrapposizione: il primo pezzo arriva fino in fondo.
    p.shifts[0].items[0].trip.to.reset();
    EXPECT_TRUE(segnala(p, Check::TripInTwoShifts, 0, "turni.json", "/turni/1/elementi/0"));
}

TEST(Turni, K_CTR_054_pezzo_di_corsa_che_non_si_trova) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-FERIALI", {pezzo("C-LIN-533", "F-SORGENTA", "")}, ShiftVehicleMode::Specific, "ALFA-X30");
    p.shifts[0].items[0].trip.from->src = dove("turni.json", "/turni/0/elementi/0/da");
    EXPECT_TRUE(segnala(p, Check::ShiftPartInvalid, 0, "turni.json", "/turni/0/elementi/0/da"));
    p.shifts[0].items[0].trip.from.reset();
    p.shifts[0].items[0].trip.to = TripPoint{Ref{"F-STAZIONE", {}}, std::nullopt, dove("turni.json", "/turni/0/elementi/0/a")};
    EXPECT_TRUE(segnala(p, Check::ShiftPartInvalid, 2, "turni.json", "/turni/0/elementi/0/a"));   // finisce dove comincia
}

// --- Altri controlli sui turni --------------------------------------------------------------------------------

TEST(Turni, K_CTR_055_elementi_fuori_ordine_o_sovrapposti) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-LV", {attivita("PULIZIA", "L-RIMESSA-ALFA", "09:00", "10:00"), attivita("PRESA", "L-RIMESSA-ALFA", "08:00", "08:30")});
    EXPECT_TRUE(segnala(p, Check::ShiftItemsOrder, 0, "turni.json", "/turni/0/elementi/1"));
    Package q = base();
    turno(q, "T", "ALFA", "CAL-LV", {attivita("PRESA", "L-RIMESSA-ALFA", "08:00", "09:30"), attivita("PULIZIA", "L-RIMESSA-ALFA", "09:00", "10:00")});
    EXPECT_TRUE(segnala(q, Check::ShiftItemsOrder, 1, "turni.json", "/turni/0/elementi/1"));
}

TEST(Turni, K_CTR_056_mezzo_incoerente_con_il_ruolo) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::None);
    EXPECT_TRUE(segnala(p, Check::ShiftVehicleInconsistent, 0, "turni.json", "/turni/0/mezzo"));
    turno(p, "ASS", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01", ShiftRole::Escort);
    EXPECT_TRUE(segnala(p, Check::ShiftVehicleInconsistent, 1, "turni.json", "/turni/1/mezzo"));
    Package q = base();
    turno(q, "ABITUALE", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Usual);
    rotazione(q, "ROT", 1, "2026-10-12", {{"P-C", {"ABITUALE"}}});   // P-C non ha un mezzo abituale
    EXPECT_TRUE(segnala(q, Check::ShiftVehicleInconsistent, 2, "rotazioni.json", "/rotazioni/0/righe/0"));
}

TEST(Turni, K_CTR_057_rotazione_scritta_male) {
    Package p = base();
    turno_semplice(p, "T", "CAL-TUTTI", "07:00", "12:00");
    rotazione(p, "ROT", 3, "2026-10-12", {{"P-A", {"T", "R"}}, {"P-A", {"R", "T", "R"}}});
    EXPECT_TRUE(segnala(p, Check::RotationMalformed, 0, "rotazioni.json", "/rotazioni/0/righe/0"));
    EXPECT_TRUE(segnala(p, Check::RotationMalformed, 1, "rotazioni.json", "/rotazioni/0/righe/1"));
    Package q = base();
    turno_semplice(q, "T", "CAL-TUTTI", "07:00", "12:00");
    rotazione(q, "ROT", 1, "2026-10-12", {{"P-A", {"T"}}}, RotationUnit::Month);
    EXPECT_TRUE(segnala(q, Check::RotationMalformed, 2, "rotazioni.json", "/rotazioni/0"));
}

TEST(Turni, K_CTR_058_assegnazione_in_un_giorno_senza_turno) {
    Package p = base();
    turno_semplice(p, "T", "CAL-LV", "07:00", "12:00");
    del_giorno(p, "2026-10-17", "T", "P-A");   // sabato
    del_giorno(p, "2028-01-10", "T", "P-A");   // fuori dal periodo
    EXPECT_TRUE(segnala(p, Check::AssignmentOnDayWithoutShift, 1, "assegnazioni.json", "/assegnazioni/0"));
    EXPECT_TRUE(segnala(p, Check::AssignmentOnDayWithoutShift, 0, "assegnazioni.json", "/assegnazioni/1"));
}

TEST(Turni, K_CTR_059_ricovero_esterno_con_l_impostazione_mai) {
    Package p = base();
    p.company_settings[0].shelter = ShelterNearHome::Never;
    Shift& s = turno_semplice(p, "T", "CAL-LV", "07:00", "12:00");
    s.end_place = Ref{"PR-01", dove("turni.json", "/turni/0/fine")};
    EXPECT_TRUE(segnala(p, Check::ShiftAtShelterNever, 0, "turni.json", "/turni/0/fine"));
    p.company_settings[0].shelter = ShelterNearHome::Allowed;
    EXPECT_EQ(quanti(p, Check::ShiftAtShelterNever), 0);
}

TEST(Turni, K_CTR_060_periodo_troppo_lungo) {
    Package p = base();
    turno_semplice(p, "T", "CAL-TUTTI", "07:00", "12:00");
    p.manifest.period.to = giorno("2030-08-31");
    EXPECT_TRUE(segnala(p, Check::PeriodTooLongForShifts, 0, "pacchetto.json", ""));
}

TEST(Turni, K_CTR_063_persona_in_due_turni_che_si_sovrappongono_anche_a_mezzanotte) {
    Package p = base();
    turno_semplice(p, "SERA", "CAL-TUTTI", "20:00", "24:30");
    turno_semplice(p, "ALBA", "CAL-TUTTI", "00:15", "06:00");
    rotazione(p, "ROT", 2, "2026-10-12", {{"P-B", {"SERA", "ALBA"}}});
    // Il 12/10 SERA finisce alle 00:30 del 13; il 13/10 ALBA comincia alle 00:15: a cavallo della mezzanotte.
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::PersonOverlap, 1, "rotazioni.json", "/rotazioni/0/righe/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    // Anche nello stesso giorno: due turni sovrapposti dati da rotazione e assegnazione.
    Package q = base();
    turno_semplice(q, "A", "CAL-LV", "07:00", "12:00");
    turno_semplice(q, "B", "CAL-LV", "11:00", "15:00");
    del_giorno(q, "2026-10-15", "A", "P-C");
    del_giorno(q, "2026-10-15", "B", "P-C");
    EXPECT_TRUE(segnala(q, Check::PersonOverlap, 0, "assegnazioni.json", "/assegnazioni/1"));
}

TEST(Turni, K_CTR_064_mezzo_in_due_turni_che_si_sovrappongono) {
    Package p = base();
    turno(p, "T1", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    turno(p, "T2", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-2")}, ShiftVehicleMode::Specific, "ALFA-01");
    EXPECT_TRUE(segnala(p, Check::VehicleOverlap, 0, "turni.json", "/turni/0/mezzo"));   // T1 comincia dopo T2
    // Un'attivita' che non usa il mezzo (spostamento a piedi) non lo impegna.
    Package q = base();
    turno(q, "T1", "ALFA", "CAL-FERIALI", {pezzo("C-LIN-533", "", "F-CASTELROVO-ALTO"), attivita("A-PIEDI", "F-CASTELROVO-ALTO", "08:08", "08:40")},
          ShiftVehicleMode::Specific, "ALFA-X30");
    turno(q, "T2", "ALFA", "CAL-FERIALI", {pezzo("C-LIN-533", "F-CASTELROVO-ALTO", "")}, ShiftVehicleMode::Specific, "ALFA-X30");
    EXPECT_EQ(quanti(q, Check::VehicleOverlap), 0);
}

TEST(Turni, K_CTR_066_due_servizi_fissi_o_due_assegnazioni) {
    Package p = base();
    turno_semplice(p, "T", "CAL-LV", "07:00", "12:00");
    fisso(p, "SF-1", "T", "P-A", "CAL-LV");
    fisso(p, "SF-2", "T", "P-B", "CAL-MER");
    del_giorno(p, "2026-10-15", "T", "P-C");
    del_giorno(p, "2026-10-15", "T", "", "ALFA-01");
    EXPECT_TRUE(segnala(p, Check::TwoAssignmentsSameShift, 0, "servizi_fissi.json", "/servizi_fissi/1"));
    EXPECT_TRUE(segnala(p, Check::TwoAssignmentsSameShift, 1, "assegnazioni.json", "/assegnazioni/1"));
}

TEST(Turni, K_CTR_068_autista_senza_abilitazione_o_autorizzazione) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-A", {"T"}}});
    EXPECT_EQ(quanti(p, Check::DriverWithoutLicence), 0);
    // Senza la D (e senza la D1): sempre sbagliato, errore (non un fatto con le date).
    Person& a = per_codice(p.persons, "P-A");
    a.licences.erase(a.licences.begin() + 1);
    EXPECT_TRUE(segnala(p, Check::DriverWithoutLicence, 0, "rotazioni.json", "/rotazioni/0/righe/0"));
    // Con la D1: basta per un mezzo di 16 posti lungo 6,2 m (ALFA-01), non per un mezzo senza lunghezza.
    a.licences.push_back({Ref{"D1", {}}, std::nullopt, "", {}});
    EXPECT_EQ(quanti(p, Check::DriverWithoutLicence), 0);
    p.shifts[0].vehicle = Ref{"ALFA-02", {}};
    EXPECT_TRUE(segnala(p, Check::DriverWithoutLicence, 0, "rotazioni.json", "/rotazioni/0/righe/0"));
    // Patente scaduta: un fatto con la data, avviso e turno scoperto.
    Package q = base();
    turno(q, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    rotazione(q, "ROT", 1, "2026-10-12", {{"P-A", {"T"}}});
    per_codice(q.persons, "P-A").licences[1].expiry = giorno("2027-05-31");
    EXPECT_TRUE(segnala(q, Check::RecurringShiftPersonFact, 3, "persone.json", "/persone/0/abilitazioni/1"));
    EXPECT_EQ(quanti(q, Check::DriverWithoutLicence), 0);
    // Servizio disabili (R-TN): serve l'autorizzazione del committente (P-F non ha il cartellino).
    Package r = base();
    turno(r, "DIS", "ALFA", "CAL-CENTRO", {corsa("C-DIS-1")}, ShiftVehicleMode::Specific, "ALFA-03");
    rotazione(r, "ROT", 1, "2026-09-01", {{"P-F", {"DIS"}}});
    EXPECT_TRUE(segnala(r, Check::DriverWithoutLicence, 1, "rotazioni.json", "/rotazioni/0/righe/0"));
    r.rotations[0].rows[0].person = Ref{"P-D", {}};   // P-D ha il cartellino
    EXPECT_EQ(quanti(r, Check::DriverWithoutLicence), 0);
}

TEST(Turni, K_CTR_069_mezzo_senza_l_uso_richiesto) {
    Package p = base();
    turno(p, "LINEA", "ALFA", "CAL-FERIALI", {corsa("C-LIN-533")}, ShiftVehicleMode::Specific, "ALFA-01");   // ALFA-01: noleggio
    rotazione(p, "ROT", 1, "2026-09-01", {{"P-A", {"LINEA"}}});
    EXPECT_TRUE(segnala(p, Check::VehicleWithoutUseForService, 0, "turni.json", "/turni/0/mezzo"));
    // Con una distrazione valida tutto l'anno: va bene. Scaduta: fatto con la data, avviso e senza mezzo.
    Vehicle& v = per_codice(p.vehicles, "ALFA-01");
    v.use_authorizations.push_back({Ref{"LINEA", {}}, "Provincia", "n. 1", p.manifest.period, "", dove("mezzi.json", "/mezzi/0/autorizzazioni_uso/0")});
    EXPECT_EQ(quanti(p, Check::VehicleWithoutUseForService), 0);
    EXPECT_EQ(quanti(p, Check::RecurringShiftVehicleFact, 1), 0);   // (la variante 0 e' la revisione del 19-20/10)
    v.use_authorizations[0].period.to = giorno("2027-02-28");
    EXPECT_TRUE(segnala(p, Check::RecurringShiftVehicleFact, 1, "mezzi.json", "/mezzi/0/autorizzazioni_uso/0"));
    EXPECT_EQ(quanti(p, Check::VehicleWithoutUseForService), 0);
    // Scolastico per il regolamento R-TN: linea o noleggio, quindi ALFA-01 va bene.
    Package q = base();
    turno(q, "SCUOLA", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    EXPECT_EQ(quanti(q, Check::VehicleWithoutUseForService), 0);
}

TEST(Turni, K_CTR_071_persona_senza_competenza) {
    Package p = base();
    turno(p, "DIS", "ALFA", "CAL-CENTRO", {corsa("C-DIS-1")}, ShiftVehicleMode::Specific, "ALFA-03");   // S-DIS chiede PEDANA
    rotazione(p, "ROT", 1, "2026-09-01", {{"P-D", {"DIS"}}});
    EXPECT_EQ(quanti(p, Check::PersonWithoutCompetence), 0);
    per_codice(p.persons, "P-D").competences = {Ref{"ELETTRICO", {}}};
    EXPECT_TRUE(segnala(p, Check::PersonWithoutCompetence, 0, "rotazioni.json", "/rotazioni/0/righe/0"));
    // Competenza richiesta dal turno.
    Package q = base();
    Shift& s = turno_semplice(q, "T", "CAL-LV", "07:00", "12:00");
    s.competences = {Ref{"LINEA-B610", {}}};
    rotazione(q, "ROT", 1, "2026-10-12", {{"P-D", {"T"}}});
    EXPECT_TRUE(segnala(q, Check::PersonWithoutCompetence, 0, "rotazioni.json", "/rotazioni/0/righe/0"));
}

TEST(Turni, K_CTR_072_mezzo_contro_il_vincolo_di_destinazione) {
    Package p = base();
    turno(p, "SCUOLA", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-X30");
    per_codice(p.vehicles, "ALFA-X30").constraint = DestinationConstraint{{ServiceKind::Line}, {}, giorno("2026-12-31"), "Contributo", "", {}};
    EXPECT_TRUE(segnala(p, Check::VehicleAgainstConstraint, 0, "turni.json", "/turni/0/mezzo"));
    per_codice(p.vehicles, "ALFA-X30").constraint->until = giorno("2026-08-31");   // vincolo gia' finito
    EXPECT_EQ(quanti(p, Check::VehicleAgainstConstraint), 0);
}

TEST(Turni, K_CTR_073_persona_o_mezzo_di_un_altra_azienda) {
    Package p = base();
    turno_semplice(p, "B", "CAL-LV", "07:00", "12:00", "BETA");
    del_giorno(p, "2026-10-15", "B", "P-A");   // P-A di Alfa, senza prestito a Beta
    EXPECT_TRUE(segnala(p, Check::OtherCompanyWithoutLoan, 0, "assegnazioni.json", "/assegnazioni/0"));
    del_giorno(p, "2026-10-16", "B", "P-C");   // P-C ha il prestito a Beta
    const auto v = check_package(p);
    EXPECT_EQ(trova(v, Check::OtherCompanyWithoutLoan, 0, "assegnazioni.json", "/assegnazioni/1"), nullptr);
    // Prestito fuori dalle sue date: fatto con la data (qui un'assegnazione del giorno: errore).
    per_codice(p.persons, "P-C").loans[0].to = giorno("2026-10-15");
    EXPECT_TRUE(segnala(p, Check::DayAssignmentAgainstFact, 2, "assegnazioni.json", "/assegnazioni/1"));
}

TEST(Turni, K_CTR_074_075_blocchi) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    p.blocks.push_back({"B-1", Ref{"ALFA-02", {}}, {Ref{"C-AM-1", dove("blocchi.json", "/blocchi/0/corse/0")}}, std::nullopt, std::nullopt,
                        BlockStrength::VehicleFixed, "", dove("blocchi.json", "/blocchi/0")});
    EXPECT_TRUE(segnala(p, Check::BlockedTripOtherVehicle, 0, "turni.json", "/turni/0/mezzo"));
    p.blocks[0].vehicle = Ref{"ALFA-01", {}};
    EXPECT_EQ(quanti(p, Check::BlockedTripOtherVehicle), 0);
    p.blocks.push_back({"B-2", Ref{"ALFA-03", {}}, {Ref{"C-AM-1", dove("blocchi.json", "/blocchi/1/corse/0")}}, giorno("2026-10-01"),
                        std::nullopt, BlockStrength::AllFixed, "", dove("blocchi.json", "/blocchi/1")});
    EXPECT_TRUE(segnala(p, Check::IncompatibleBlocks, 0, "blocchi.json", "/blocchi/1/corse/0"));
}

TEST(Turni, K_CTR_076_ruolo_diverso_da_quello_del_turno) {
    Package p = base();
    turno_semplice(p, "T", "CAL-LV", "07:00", "12:00", "BETA");
    del_giorno(p, "2026-10-15", "T", "P-B4");   // assistente in un turno da autista
    EXPECT_TRUE(segnala(p, Check::PersonRoleMismatch, 0, "assegnazioni.json", "/assegnazioni/0"));
}

TEST(Turni, K_CTR_080_corsa_senza_turno_e_084_085) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::Specific, "ALFA-01");
    rotazione(p, "ROT", 1, "2026-09-01", {{"P-A", {"T"}}});
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::TripWithoutShift, 0, "corse.json", "/corse/1");   // C-AM-2
    ASSERT_NE(x, nullptr) << elenco(v);
    EXPECT_GT(giorni_di(*x).size(), 100u);   // un avviso per corsa, con tutti i giorni
    EXPECT_EQ(trova(v, Check::TripWithoutShift, 0, "corse.json", "/corse/0"), nullptr);   // C-AM-1 ha il suo turno
    EXPECT_NE(trova(v, Check::PersonWithoutShifts, 0, "persone.json", "/persone/1"), nullptr);   // P-B
    EXPECT_EQ(trova(v, Check::PersonWithoutShifts, 0, "persone.json", "/persone/4"), nullptr);   // P-NUOVO e' ipotetica
    EXPECT_NE(trova(v, Check::VehicleNeverUsed, 0, "mezzi.json", "/mezzi/1"), nullptr);          // ALFA-02
    EXPECT_EQ(trova(v, Check::VehicleNeverUsed, 0, "mezzi.json", "/mezzi/4"), nullptr);          // ALFA-X30 e' ipotetico
    per_codice(p.vehicles, "ALFA-02").spare = true;
    EXPECT_EQ(trova(check_package(p), Check::VehicleNeverUsed, 0, "mezzi.json", "/mezzi/1"), nullptr);   // di scorta
}

TEST(Turni, K_CTR_081_preferenza_non_rispettata) {
    Package p = base();
    turno_semplice(p, "DOMENICA", "CAL-TUTTI", "07:00", "12:00");
    rotazione(p, "ROT", 1, "2026-10-12", {{"P-B", {"DOMENICA"}}});   // P-B preferisce la domenica libera
    const auto v = check_package(p);
    const Problem* x = trova(v, Check::PreferenceNotRespected, 0, "persone.json", "/persone/1/preferenze/0");
    ASSERT_NE(x, nullptr) << elenco(v);
    for (Day d : giorni_di(*x)) EXPECT_EQ(weekday_of(d), Weekday::Sunday);
    EXPECT_EQ(api::definizione_controllo(x->check).livello, api::Livello::Avviso);
}

TEST(Turni, K_CTR_083_mezzo_da_assegnare) {
    Package p = base();
    turno(p, "T", "ALFA", "CAL-SCUOLA-LV", {corsa("C-AM-1")}, ShiftVehicleMode::ToAssign);
    EXPECT_TRUE(segnala(p, Check::ShiftVehicleToAssign, 0, "turni.json", "/turni/0/mezzo"));
}

TEST(Turni, K_CTR_086_087_assistente) {
    // C-ASILO-1: l'assistente sale e scende; il contratto CT-ASILO dice che lo fornisce l'azienda.
    Package p = base();
    turno(p, "ASILO", "BETA", "CAL-INFANZIA", {corsa("C-ASILO-1")}, ShiftVehicleMode::Specific, "BETA-01");
    EXPECT_TRUE(segnala(p, Check::EscortWithoutPerson, 0, "turni.json", "/turni/0/elementi/0"));
    turno(p, "ASILO-ASS", "BETA", "CAL-INFANZIA", {corsa("C-ASILO-1")}, ShiftVehicleMode::None, "", ShiftRole::Escort);
    rotazione(p, "ROT", 1, "2026-09-01", {{"P-B4", {"ASILO-ASS"}}});
    EXPECT_EQ(quanti(p, Check::EscortWithoutPerson), 0);
    const Planner piano(p);
    const auto giornata_ass = piano.day(giorno("2026-10-15"));
    const ShiftDay* ass = turno_del_giorno(giornata_ass, "ASILO-ASS");
    ASSERT_NE(ass, nullptr);
    EXPECT_EQ(ass->aboard->code, "BETA-01");   // a bordo del mezzo del turno dell'autista
    // C-DIS-1: il contratto CT-SCU dice committente; il turno dice azienda.
    Package q = base();
    turno(q, "DIS", "ALFA", "CAL-CENTRO", {corsa("C-DIS-1")}, ShiftVehicleMode::Specific, "ALFA-03");
    EXPECT_EQ(quanti(q, Check::EscortWithoutPerson), 0);   // fornito dal committente: nessuna persona dell'azienda
    q.shifts[0].items[0].trip.escort = EscortProvider::Company;
    EXPECT_TRUE(segnala(q, Check::EscortProviderDiffers, 0, "turni.json", "/turni/0/elementi/0"));
}

// Raggruppamento: lo stesso problema in molti giorni e' uno solo, con tutti i giorni.
TEST(Turni, avvisi_raggruppati_per_periodo) {
    Package p = base();
    turno_semplice(p, "T", "CAL-LV", "07:00", "12:00");
    const auto v = check_package(p);
    ASSERT_EQ(std::count_if(v.begin(), v.end(), [](const Problem& x) { return x.check == Check::ShiftUncovered; }), 1);
    const Problem* x = trova(v, Check::ShiftUncovered, 0, "turni.json", "/turni/0");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(giorni_di(*x).size(), 261u);   // tutti i giorni da lunedi' a venerdi' del periodo
    EXPECT_EQ(api::testo_giorni({giorno("2026-10-20")}), "il 2026-10-20");
    EXPECT_EQ(api::testo_giorni({giorno("2026-10-20"), giorno("2026-10-21"), giorno("2026-10-22")}), "dal 2026-10-20 al 2026-10-22 (3 giorni)");
    EXPECT_EQ(api::testo_giorni({giorno("2026-10-20"), giorno("2026-10-22"), giorno("2026-10-23")}),
              "in 3 giorni (2026-10-20, dal 2026-10-22 al 2026-10-23)");
}

// Stesso pacchetto = stesso piano e stessi problemi, nello stesso ordine.
TEST(Turni, piano_stabile) {
    Package p = base();
    turno_semplice(p, "B", "CAL-LV", "07:00", "12:00");
    turno_semplice(p, "A", "CAL-LV", "07:00", "12:00");
    const Planner piano(p);
    const auto g = piano.day(giorno("2026-10-15"));
    ASSERT_EQ(g.size(), 2u);
    EXPECT_EQ(g[0].shift->code, "A");   // stesso inizio: in ordine di codice
    const auto a = check_package(p);
    const auto b = check_package(p);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_EQ(a[i].where.pointer, b[i].where.pointer);
}
