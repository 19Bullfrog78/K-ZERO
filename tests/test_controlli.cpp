// K-Zero - prove del nucleo: ogni controllo sul contenuto (K-CTR-009 ... K-CTR-046, docs/controlli.md).
// Si parte dal pacchetto demo, che non ha errori, e si cambia un solo dato: il controllo deve segnalare
// il problema giusto nel punto giusto (file e percorso JSON).
#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "controlli.hpp"
#include "controlli_catalogo.hpp"
#include "lettore.hpp"
#include "pacchetto_demo.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

std::string elenco(const std::vector<Problem>& v) {
    std::string s;
    for (const Problem& x : v)
        s += "\n  " + api::definizione_controllo(x.check).codice + " variante " + std::to_string(x.variant) + " in " +
             x.where.file + " " + x.where.pointer;
    return s.empty() ? " nessuno" : s;
}

// Il pacchetto ha il problema c, nella forma (variante) indicata, in quel punto?
::testing::AssertionResult segnala(const Package& p, Check c, int variante, const std::string& file,
                                   const std::string& puntatore) {
    const auto v = check_package(p);
    for (const Problem& x : v)
        if (x.check == c && x.variant == variante && x.where.file == file && x.where.pointer == puntatore)
            return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << api::definizione_controllo(c).codice << " variante " << variante << " in "
                                         << file << " " << puntatore << " non segnalato. Problemi trovati:" << elenco(v);
}

int quanti(const Package& p, Check c) {
    const auto v = check_package(p);
    return static_cast<int>(std::count_if(v.begin(), v.end(), [c](const Problem& x) { return x.check == c; }));
}

const Problem* primo(const std::vector<Problem>& v, Check c) {
    for (const Problem& x : v)
        if (x.check == c) return &x;
    return nullptr;
}

// Valore di un parametro del messaggio.
const Param* parametro(const Problem& x, const std::string& nome) {
    for (const Param& p : x.params)
        if (p.name == nome) return &p;
    return nullptr;
}

Source dove(const std::string& file, const std::string& puntatore) { return Source{file, puntatore, 0, 0}; }

}  // namespace

// --- Pacchetti di esempio --------------------------------------------------------------------------------

// Il demo non ha errori: solo avvisi voluti. Dal K-007: fermata senza coordinate, tempi di sosta non definiti, punto di
// trasbordo autorizzato a voce. Dal K-008: la distrazione dell'autobus di linea e la CQC di P-C che scadono il
// 2027-06-30 (da luglio il secondo del mercoledi', che e' di P-C, e' scoperto), il primo scoperto il giorno della
// formazione di P-B, il mezzo di scorta al posto del mezzo abituale di P-E nei due giorni della revisione.
// Dal K-011 gli avvisi degli orari: le regole che si verificano solo in parte all'inizio e alla fine del periodo, il fermo
// minimo del rientro di Alfa (30 minuti) uguale alle soste che la L. 138 conta come lavoro, P-G a chiamata vicina alle 400
// giornate, il titolare P-C (L. 138 da confermare per il suo rapporto), il riposo settimanale di P-A dopo la gita, le
// giornate oltre la preferenza di Alfa (11 ore).
void solo_avvisi_voluti(const std::vector<Problem>& v) {
    ASSERT_EQ(v.size(), 31u) << elenco(v);
    const std::vector<std::tuple<Check, std::string, int>> attesi = {
        {Check::PlaceWithoutCoordinates, "/luoghi/5", 1},
        {Check::DwellTimesUndefined, "", 1},
        {Check::TransferVerbalOnly, "/luoghi/17/trasbordo", 1},
        {Check::ExpiresInPeriod, "/mezzi/6/autorizzazioni_uso/0", 1},
        {Check::ExpiresInPeriod, "/persone/2/abilitazioni/2", 1},
        {Check::RecurringShiftPersonFact, "/persone/2/abilitazioni/2", 1},
        {Check::RecurringShiftPersonFact, "/assenze/2", 1},
        {Check::PairingNotRespected, "/servizi_fissi/2", 1},
        {Check::PartialVerification, "/norme/0/regimi/0/regole/2", 1},
        {Check::PartialVerification, "/norme/0/regimi/0/regole/5", 1},
        {Check::PartialVerification, "/norme/0/regimi/1/regole/2", 1},
        {Check::PartialVerification, "/norme/0/regimi/1/regole/3", 1},
        {Check::PartialVerification, "/norme/0/regimi/2/regole/1", 1},
        {Check::PartialVerification, "/norme/0/regimi/2/regole/2", 1},
        {Check::PartialVerification, "/norme/0/regimi/4/regole/2", 1},
        {Check::ReturnStopBelowCountedStops, "/aziende/0/rientro", 1},
        {Check::DaysNearLimit, "/persone/11", 1},
        {Check::LawWorkToConfirm, "/persone/2", 2},
        {Check::LawOvertime, "/persone/2", 1},
        {Check::LawDailyRestToConfirm, "/persone/2", 3},
        {Check::LawWeeklyRestToConfirm, "/persone/0", 1},
        {Check::ChoiceNotRespected, "/persone/0", 2},
        {Check::ChoiceNotRespected, "/persone/1", 2},
        {Check::ChoiceNotRespected, "/persone/2", 3},
    };
    for (const auto& [c, puntatore, n] : attesi) {
        const Check cc = c;   // C++17: una lambda non cattura i nomi di un legame strutturato
        const std::string pp = puntatore;
        EXPECT_EQ(std::count_if(v.begin(), v.end(), [&](const Problem& x) { return x.check == cc && x.where.pointer == pp; }), n)
            << api::definizione_controllo(cc).codice << " " << pp << elenco(v);
    }
    for (const Problem& x : v) EXPECT_EQ(api::definizione_controllo(x.check).livello, api::Livello::Avviso) << elenco(v);
}

TEST(Controlli, demo_senza_errori) { solo_avvisi_voluti(check_package(demo())); }

TEST(Controlli, demo_numeri_senza_errori) {
    const auto lettura = api::leggi_pacchetto(api::CartellaSuDisco(KZERO_SORGENTI "/esempi/demo-numeri"));
    ASSERT_TRUE(lettura.problemi.empty());
    EXPECT_EQ(lettura.pacchetto.manifest.level, Level::NumbersOnly);
    solo_avvisi_voluti(check_package(lettura.pacchetto));   // gli stessi del demo
}

// Stesso pacchetto = stessi problemi, nello stesso ordine.
TEST(Controlli, ordine_stabile) {
    Package p = demo();
    p.trips[3].passages[2].time = ora("16:08");
    per_codice(p.places, "L-TRASB-PONTE").transfer.reset();
    const auto a = check_package(p);
    const auto b = check_package(p);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].check, b[i].check);
        EXPECT_EQ(a[i].where.pointer, b[i].where.pointer);
    }
}

// --- Errori ----------------------------------------------------------------------------------------------

TEST(Controlli, K_CTR_009_codice_doppio) {
    Package p = demo();
    p.trips[2].code = "C-AM-1";
    EXPECT_TRUE(segnala(p, Check::DuplicateCode, 0, "corse.json", "/corse/2"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::DuplicateCode);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "prima")->text, "corse.json /corse/0");

    Package q = demo();
    gruppo(q, "G-RM-MEDIE").code = "G-AM-MEDIE";   // i gruppi hanno codici unici in tutto il pacchetto
    EXPECT_TRUE(segnala(q, Check::DuplicateCode, 0, "servizi.json", "/servizi/1/domanda/gruppi/0"));
}

TEST(Controlli, K_CTR_010_riferimento_inesistente) {
    Package p = demo();
    p.trips[0].passages[1].place.code = "F-PRADELO";
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 0, "corse.json", "/corse/0/passaggi/1/luogo"));
    Package q = demo();
    q.contracts[2].regulation.code = "R-XX";
    EXPECT_TRUE(segnala(q, Check::UnknownReference, 0, "contratti.json", "/contratti/2/regolamento"));
}

TEST(Controlli, K_CTR_011_luogo_del_tipo_sbagliato) {
    Package p = demo();
    p.schools[0].place.code = "F-PRADELLO";
    EXPECT_TRUE(segnala(p, Check::WrongPlaceType, 0, "scuole.json", "/scuole/0/luogo"));
    Package q = demo();
    q.depots[0].place.code = "F-PRADELLO";
    EXPECT_TRUE(segnala(q, Check::WrongPlaceType, 1, "rimesse.json", "/rimesse/0/luogo"));
}

TEST(Controlli, K_CTR_012_fine_prima_dell_inizio) {
    Package p = demo();
    per_codice(p.calendars, "CAL-CENTRO").range.to = giorno("2026-08-31");
    EXPECT_TRUE(segnala(p, Check::InvertedPeriod, 0, "calendari.json", "/calendari/3"));
    Package q = demo();
    q.schools[0].timetables[0].end = ora("07:00");
    EXPECT_TRUE(segnala(q, Check::InvertedPeriod, 1, "scuole.json", "/scuole/0/orari/0"));
}

TEST(Controlli, K_CTR_013_data_fuori_dal_periodo) {
    Package p = demo();
    per_codice(p.calendars, "CAL-CENTRO").added[0].day = giorno("2027-08-15");
    EXPECT_TRUE(segnala(p, Check::DateOutsideCalendar, 0, "calendari.json", "/calendari/3/date_aggiunte/0"));
    Package q = demo();
    per_codice(q.calendars, "CAL-CENTRO").removed[0].day = giorno("2027-08-15");
    EXPECT_TRUE(segnala(q, Check::DateOutsideCalendar, 1, "calendari.json", "/calendari/3/date_tolte/0"));
    Package r = demo();
    per_codice(r.calendars, "CAL-CENTRO").suspensions[0].range.to = giorno("2027-08-05");
    EXPECT_TRUE(segnala(r, Check::DateOutsideCalendar, 2, "calendari.json", "/calendari/3/sospensioni/0"));
}

TEST(Controlli, K_CTR_014_data_contraddittoria) {
    Package p = demo();
    per_codice(p.calendars, "CAL-CENTRO").added[0].day = giorno("2027-06-02");   // e' anche tra le date tolte
    EXPECT_TRUE(segnala(p, Check::ContradictoryDate, 0, "calendari.json", "/calendari/3/date_aggiunte/0"));
    Package q = demo();
    per_codice(q.calendars, "CAL-CENTRO").added[0].day = giorno("2026-12-28");   // cade nella chiusura del centro
    EXPECT_TRUE(segnala(q, Check::ContradictoryDate, 1, "calendari.json", "/calendari/3/date_aggiunte/0"));
}

TEST(Controlli, K_CTR_015_calendario_senza_giorni) {
    Package p = demo();
    per_codice(p.calendars, "CAL-FERIALI").days = Weekdays{};
    EXPECT_TRUE(segnala(p, Check::CalendarWithoutDays, 0, "calendari.json", "/calendari/4"));
    // Con le sole date aggiunte un calendario vale lo stesso (i calendari che lo usano come base e non hanno quel giorno
    // della settimana, come i feriali da lunedi' a venerdi', restano senza giorni: e' giusto che siano segnalati).
    per_codice(p.calendars, "CAL-FERIALI").added.push_back({giorno("2026-10-10"), dove("calendari.json", "/x")});
    EXPECT_FALSE(segnala(p, Check::CalendarWithoutDays, 0, "calendari.json", "/calendari/4"));
}

TEST(Controlli, K_CTR_016_calendario_circolare) {
    Package p = demo();
    per_codice(p.calendars, "CAL-SCUOLA").base = Ref{"CAL-INFANZIA", dove("calendari.json", "/calendari/0/base")};
    EXPECT_TRUE(segnala(p, Check::CalendarCycle, 0, "calendari.json", "/calendari/0/base"));
    EXPECT_EQ(quanti(p, Check::CalendarCycle), 3);   // i tre calendari del giro
    // Nemmeno i calendari che usano quelli del giro (dal K-008: i martedi' e venerdi' di scuola) sono "senza giorni".
    EXPECT_EQ(quanti(p, Check::CalendarWithoutDays), 0);
}

// Caso reale: "16.08" scritto tra 12.05 e 12.19. L'orario fuori posto e' quello di mezzo.
// Calendario che si combina con se stesso attraverso i calendari aggiunti o tolti (anche insieme alle basi).
TEST(Controlli, K_CTR_049_calendario_combinato_con_se_stesso) {
    Package p = demo();
    per_codice(p.calendars, "CAL-FERIALI").adds = {
        {CalendarLinkKind::Calendar, Ref{"CAL-CENTRO-GIO", dove("calendari.json", "/calendari/4/aggiunge/calendari/0")}}};
    per_codice(p.calendars, "CAL-CENTRO").removes = {
        {CalendarLinkKind::Calendar, Ref{"CAL-FERIALI", dove("calendari.json", "/calendari/3/toglie/calendari/0")}}};
    // Giro: CAL-FERIALI aggiunge CAL-CENTRO-GIO, che ha base CAL-CENTRO, che toglie CAL-FERIALI.
    EXPECT_TRUE(segnala(p, Check::CalendarCombinationCycle, 0, "calendari.json", "/calendari/4/aggiunge/calendari/0"));
    EXPECT_TRUE(segnala(p, Check::CalendarCombinationCycle, 0, "calendari.json", "/calendari/3/toglie/calendari/0"));
    EXPECT_EQ(quanti(p, Check::CalendarCombinationCycle), 3);   // i tre calendari del giro
    EXPECT_EQ(quanti(p, Check::CalendarCycle), 0);              // il giro non e' fatto di sole basi
    EXPECT_EQ(quanti(p, Check::CalendarWithoutDays), 0);        // niente avvisi in piu' sui calendari del giro
    // Direttamente: un calendario che aggiunge se stesso.
    Package q = demo();
    per_codice(q.calendars, "CAL-FERIALI").adds = {
        {CalendarLinkKind::Calendar, Ref{"CAL-FERIALI", dove("calendari.json", "/calendari/4/aggiunge/calendari/0")}}};
    EXPECT_TRUE(segnala(q, Check::CalendarCombinationCycle, 0, "calendari.json", "/calendari/4/aggiunge/calendari/0"));
    EXPECT_EQ(quanti(q, Check::CalendarCombinationCycle), 1);
    // Controprova: aggiungere o togliere calendari senza giri va bene.
    Package r = demo();
    per_codice(r.calendars, "CAL-FERIALI").removes = {{CalendarLinkKind::Calendar, Ref{"CAL-CENTRO-GIO", {}}}};
    EXPECT_EQ(quanti(r, Check::CalendarCombinationCycle), 0);
}

// Un calendario che aggiunge o toglie una festivita' o un calendario che non esistono.
TEST(Controlli, K_CTR_010_festivita_inesistente) {
    Package p = demo();
    per_codice(p.calendars, "CAL-FERIALI").removes = {
        {CalendarLinkKind::Holidays, Ref{"FEST-XX", dove("calendari.json", "/calendari/4/toglie/festivita/0")}}};
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 0, "calendari.json", "/calendari/4/toglie/festivita/0"));
    // Un codice di calendario non vale come festivita' (e viceversa): ogni tipo ha i suoi codici.
    per_codice(p.calendars, "CAL-FERIALI").removes = {
        {CalendarLinkKind::Holidays, Ref{"CAL-SCUOLA", dove("calendari.json", "/calendari/4/toglie/festivita/0")}}};
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 0, "calendari.json", "/calendari/4/toglie/festivita/0"));
}

// K-008: riferimenti dentro gli elenchi delle norme e delle aziende.
TEST(Controlli, K_CTR_010_elenchi_di_norme_e_aziende) {
    Package p = demo();
    per_codice(p.norms, "N-IT").requirements[2].alternatives[1].licences[0] =
        Ref{"D2", dove("norme.json", "/norme/0/requisiti_guida/2/alternative/1/abilitazioni/0")};
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 1, "norme.json", "/norme/0/requisiti_guida/2/alternative/1/abilitazioni/0"));
    Package q = demo();
    per_codice(q.vehicles, "ALFA-01").use = Ref{"NOLEGGIO", dove("mezzi.json", "/mezzi/0/uso")};
    EXPECT_TRUE(segnala(q, Check::UnknownReference, 1, "mezzi.json", "/mezzi/0/uso"));
    Package r = demo();
    per_codice(r.services, "S-DIS").competences[0] = Ref{"PEDANE", dove("servizi.json", "/servizi/3/competenze_richieste/0")};
    EXPECT_TRUE(segnala(r, Check::UnknownReference, 2, "servizi.json", "/servizi/3/competenze_richieste/0"));
    // Una competenza di un'altra azienda non vale: ogni azienda ha il suo elenco.
    per_codice(r.services, "S-DIS").competences[0] = Ref{"INFANZIA", dove("servizi.json", "/servizi/3/competenze_richieste/0")};
    EXPECT_TRUE(segnala(r, Check::UnknownReference, 2, "servizi.json", "/servizi/3/competenze_richieste/0"));
    Package s = demo();
    per_codice(s.regulations, "R-TN").vehicle_uses->values[0].uses[1] = Ref{"TAXI", dove("regolamenti.json", "/x")};
    EXPECT_TRUE(segnala(s, Check::UnknownReference, 0, "regolamenti.json", "/x"));
    Package u = demo();
    u.company_settings[1].company = Ref{"GAMMA", dove("aziende.json", "/aziende/1/azienda")};
    u.company_settings[1].norms = Ref{"N-DE", dove("aziende.json", "/aziende/1/norme")};
    EXPECT_TRUE(segnala(u, Check::UnknownReference, 0, "aziende.json", "/aziende/1/azienda"));
    EXPECT_TRUE(segnala(u, Check::UnknownReference, 0, "aziende.json", "/aziende/1/norme"));
}

TEST(Controlli, K_CTR_009_codici_doppi_di_norme_e_aziende) {
    Package p = demo();
    p.company_settings[1].company.code = "ALFA";
    EXPECT_TRUE(segnala(p, Check::DuplicateCode, 0, "aziende.json", "/aziende/1"));
    Package q = demo();
    q.company_settings[0].activities[1].code = "PRESA";
    EXPECT_TRUE(segnala(q, Check::DuplicateCode, 0, "aziende.json", "/aziende/0/attivita/1"));
    Package r = demo();
    r.norms[0].uses[1].code = "LINEA";
    EXPECT_TRUE(segnala(r, Check::DuplicateCode, 0, "norme.json", "/norme/0/usi/1"));
}

// Distrazione: autorizzazione a un uso in piu' che scade dentro il periodo del pacchetto (avviso).
TEST(Controlli, K_CTR_050_distrazione_che_scade_nel_periodo) {
    Package p = demo();
    Vehicle& v = per_codice(p.vehicles, "ALFA-X30");
    v.use_authorizations.push_back({Ref{"NCC", {}}, "Provincia", "n. 1", {giorno("2026-03-01"), giorno("2027-02-28")}, "",
                                    dove("mezzi.json", "/mezzi/4/autorizzazioni_uso/0")});
    EXPECT_TRUE(segnala(p, Check::ExpiresInPeriod, 0, "mezzi.json", "/mezzi/4/autorizzazioni_uso/0"));
    // Valida fino all'ultimo giorno del periodo (o oltre): nessun avviso.
    v.use_authorizations[0].period.to = giorno("2027-08-31");
    EXPECT_FALSE(segnala(p, Check::ExpiresInPeriod, 0, "mezzi.json", "/mezzi/4/autorizzazioni_uso/0"));
    // Date rovesciate: errore K-CTR-012.
    v.use_authorizations[0].period = {giorno("2027-02-28"), giorno("2026-03-01")};
    EXPECT_TRUE(segnala(p, Check::InvertedPeriod, 0, "mezzi.json", "/mezzi/4/autorizzazioni_uso/0"));
}

// Mezzo senza l'uso della carta di circolazione, quando le norme dell'azienda lo usano (avviso).
TEST(Controlli, K_CTR_051_mezzo_senza_uso) {
    Package p = demo();
    per_codice(p.vehicles, "BETA-01").use.reset();
    EXPECT_TRUE(segnala(p, Check::VehicleWithoutUse, 0, "mezzi.json", "/mezzi/5"));
    EXPECT_EQ(quanti(p, Check::VehicleWithoutUse), 1);
}

// Azienda che scrive l'uso dei mezzi ma non indica le norme del paese (avviso, una volta per azienda).
TEST(Controlli, K_CTR_052_azienda_senza_norme) {
    Package p = demo();
    p.company_settings[0].norms.reset();
    EXPECT_TRUE(segnala(p, Check::CompanyWithoutNorms, 0, "pacchetto.json", "/aziende/0"));
    EXPECT_EQ(quanti(p, Check::CompanyWithoutNorms), 1);
    EXPECT_EQ(quanti(p, Check::UnknownReference), 0);   // senza norme gli usi non si controllano
    // Senza impostazioni per l'azienda: stesso avviso.
    Package q = demo();
    q.company_settings.erase(q.company_settings.begin() + 1);
    EXPECT_TRUE(segnala(q, Check::CompanyWithoutNorms, 0, "pacchetto.json", "/aziende/1"));
}

// --- Persone, assenze, fermi (K-008) -----------------------------------------------------------------------

TEST(Controlli, K_CTR_009_persona_doppia) {
    Package p = demo();
    p.persons[1].code = "P-A";
    EXPECT_TRUE(segnala(p, Check::DuplicateCode, 0, "persone.json", "/persone/1"));
}

TEST(Controlli, K_CTR_010_riferimenti_delle_persone) {
    Package p = demo();
    Person& x = per_codice(p.persons, "P-A");
    x.licences[1].type = Ref{"D2", dove("persone.json", "/persone/0/abilitazioni/1/tipo")};
    x.competences[0] = Ref{"PEDALE", dove("persone.json", "/persone/0/competenze/0")};
    x.usual_vehicle = Ref{"ALFA-99", dove("persone.json", "/persone/0/mezzo_abituale")};
    x.base_place = Ref{"L-NON-ESISTE", dove("persone.json", "/persone/0/presa_servizio")};
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 1, "persone.json", "/persone/0/abilitazioni/1/tipo"));
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 2, "persone.json", "/persone/0/competenze/0"));
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 0, "persone.json", "/persone/0/mezzo_abituale"));
    EXPECT_TRUE(segnala(p, Check::UnknownReference, 0, "persone.json", "/persone/0/presa_servizio"));
    // Il codice scritto in un dato personale e' segnato come personale: compare solo con il parametro.
    for (const Problem& q : check_package(p))
        if (q.where.pointer == "/persone/0/abilitazioni/1/tipo") {
            const Param* c = parametro(q, "codice");
            ASSERT_NE(c, nullptr);
            EXPECT_EQ(c->sensitivity, Param::Sensitivity::Personal);
        }
    Package q = demo();
    q.absences[0].type = Ref{"MALATTIA", dove("assenze.json", "/assenze/0/tipo")};
    EXPECT_TRUE(segnala(q, Check::UnknownReference, 2, "assenze.json", "/assenze/0/tipo"));
    q.absences[0].person = Ref{"P-ZZ", dove("assenze.json", "/assenze/0/persona")};
    EXPECT_TRUE(segnala(q, Check::UnknownReference, 0, "assenze.json", "/assenze/0/persona"));
    Package r = demo();
    r.stops[0].type = Ref{"OFFICINA", dove("fermi.json", "/fermi/0/tipo")};   // tipo di Beta, non di Alfa
    EXPECT_TRUE(segnala(r, Check::UnknownReference, 2, "fermi.json", "/fermi/0/tipo"));
    Package s = demo();
    per_codice(s.persons, "P-D").authorizations[0].client = Ref{"XYZ", dove("persone.json", "/persone/3/autorizzazioni/0/committente")};
    EXPECT_TRUE(segnala(s, Check::UnknownReference, 0, "persone.json", "/persone/3/autorizzazioni/0/committente"));
}

TEST(Controlli, K_CTR_012_periodi_rovesciati_di_persone_assenze_fermi) {
    Package p = demo();
    p.absences[0].days = {giorno("2026-10-23"), giorno("2026-10-22")};
    p.stops[0].start = ora("12:00");
    p.stops[0].end = ora("08:00");
    per_codice(p.persons, "P-D").availability[0].end = ora("06:00");
    EXPECT_TRUE(segnala(p, Check::InvertedPeriod, 0, "assenze.json", "/assenze/0"));
    EXPECT_TRUE(segnala(p, Check::InvertedPeriod, 1, "fermi.json", "/fermi/0"));
    EXPECT_TRUE(segnala(p, Check::InvertedPeriod, 1, "persone.json", "/persone/3/disponibilita/0"));
}

// "Solo numeri": restano ruolo, abilitazioni, competenze, disponibilita'; non nomi, case e note delle persone.
TEST(Controlli, K_CTR_019_persone_in_solo_numeri) {
    Package p = demo();
    p.manifest.level = Level::NumbersOnly;
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 2, "persone.json", "/persone/0/nome"));
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 3, "persone.json", "/persone/0/casa"));
    per_codice(p.persons, "P-NUOVO").note = "Da assumere";
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 4, "persone.json", "/persone/4/nota"));
    EXPECT_FALSE(segnala(p, Check::NumbersOnlyPersonalData, 2, "persone.json", "/persone/4/nome"));   // senza nome
}

TEST(Controlli, K_CTR_050_abilitazione_o_autorizzazione_che_scade) {
    Package p = demo();
    EXPECT_TRUE(segnala(p, Check::ExpiresInPeriod, 1, "persone.json", "/persone/2/abilitazioni/2"));   // gia' nel demo
    per_codice(p.persons, "P-A").licences[1].expiry = giorno("2027-06-30");
    per_codice(p.persons, "P-D").authorizations[0].expiry = giorno("2027-01-31");
    EXPECT_TRUE(segnala(p, Check::ExpiresInPeriod, 1, "persone.json", "/persone/0/abilitazioni/1"));
    EXPECT_TRUE(segnala(p, Check::ExpiresInPeriod, 2, "persone.json", "/persone/3/autorizzazioni/0"));
    // Le altre scadenze sono dopo il periodo: oltre a queste tre resta solo la distrazione dell'autobus di linea.
    EXPECT_EQ(quanti(p, Check::ExpiresInPeriod), 4);
}

TEST(Controlli, K_CTR_052_abilitazioni_senza_norme) {
    Package p = demo();
    p.company_settings[1].norms.reset();   // Beta: mezzi con l'uso e persone con le abilitazioni
    for (Vehicle& v : p.vehicles)
        if (v.company.code == "BETA") v.use.reset();
    EXPECT_TRUE(segnala(p, Check::CompanyWithoutNorms, 0, "pacchetto.json", "/aziende/1"));
    for (Person& x : p.persons)
        if (x.company.code == "BETA") x.licences.clear();
    EXPECT_TRUE(segnala(p, Check::CompanyWithoutNorms, 0, "pacchetto.json", "/aziende/1"));   // restano i turni con i mezzi
    std::vector<Shift> turni;
    for (const Shift& t : p.shifts)
        if (t.company.code != "BETA" || t.vehicle_mode == ShiftVehicleMode::None) turni.push_back(t);
    p.shifts = turni;
    EXPECT_EQ(quanti(p, Check::CompanyWithoutNorms), 0);
}

TEST(Controlli, K_CTR_017_orario_che_torna_indietro) {
    Package p = demo();
    p.trips[3].passages[2].time = ora("16:08");
    EXPECT_TRUE(segnala(p, Check::TimeGoesBack, 0, "corse.json", "/corse/3/passaggi/2"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::TimeGoesBack);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "orario")->number, ora("16:08"));
    EXPECT_EQ(parametro(*x, "precedente")->number, ora("12:05"));
    EXPECT_EQ(parametro(*x, "successivo")->number, ora("12:19"));
    EXPECT_EQ(quanti(p, Check::TimeGoesBack), 1);

    Package q = demo();
    q.trips[3].passages[1].time = ora("11:50");   // prima del passaggio precedente (11:58)
    EXPECT_TRUE(segnala(q, Check::TimeGoesBack, 1, "corse.json", "/corse/3/passaggi/1"));
    Package r = demo();
    r.trips[5].passages[3].departure = ora("08:10");   // partenza prima dell'arrivo (08:20)
    EXPECT_TRUE(segnala(r, Check::TimeGoesBack, 2, "corse.json", "/corse/5/passaggi/3"));
    // Dopo mezzanotte l'orario continua a crescere: 23:48, 24:05 e' in ordine.
    EXPECT_EQ(quanti(demo(), Check::TimeGoesBack), 0);
}

TEST(Controlli, K_CTR_018_fermata_ambigua) {
    Package p = demo();
    per_codice(p.places, "F-CASTELROVO").name = "Pradello - piazza";
    EXPECT_TRUE(segnala(p, Check::AmbiguousStop, 0, "luoghi.json", "/luoghi/4"));
    Package q = demo();
    per_codice(q.places, "F-STAZIONE").external[0].code = "5101";
    EXPECT_TRUE(segnala(q, Check::AmbiguousStop, 1, "luoghi.json", "/luoghi/9"));
    // Una scuola con il nome di una fermata non e' un'ambiguita' tra fermate.
    Package r = demo();
    per_codice(r.places, "L-MEDIE").name = "Pradello, Piazza";
    EXPECT_EQ(quanti(r, Check::AmbiguousStop), 0);
}

TEST(Controlli, K_CTR_019_dati_personali_in_solo_numeri) {
    Package p = demo();
    p.manifest.level = Level::NumbersOnly;
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 0, "luoghi.json", "/luoghi_privati/0"));
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 1, "servizi.json", "/servizi/3/domanda/gruppi/0/passeggeri/0"));
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 5, "rotazioni.json", "/rotazioni/0/nota"));
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 6, "servizi_fissi.json", "/servizi_fissi/0/nota"));
    EXPECT_TRUE(segnala(p, Check::NumbersOnlyPersonalData, 7, "assegnazioni.json", "/assegnazioni/2/nota"));
    // 5 luoghi privati, 3 gruppi con passeggeri; dal K-008 anche 11 nomi e 5 case di persone, 2 note di rotazioni,
    // 2 di servizi fissi e 8 di assegnazioni del singolo giorno; dal K-011 una nota in piu' (la sostituzione di P-A il
    // 2026-11-13, per il riposo settimanale nella settimana della gita).
    EXPECT_EQ(quanti(p, Check::NumbersOnlyPersonalData), 37);
}

TEST(Controlli, K_CTR_020_persone_a_bordo_sotto_zero) {
    Package p = demo();
    p.trips[0].passages[3].persons[0].alighting = 17;   // a bordo ce ne sono 16
    EXPECT_TRUE(segnala(p, Check::NegativeOnBoard, 0, "corse.json", "/corse/0/passaggi/3"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::NegativeOnBoard);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "scendono")->number, 17);
    EXPECT_EQ(parametro(*x, "a_bordo")->number, 16);
    // Il trasbordo conta: senza la ricezione, a L-CENTRO scendono 3 persone ma a bordo ce ne sono 2.
    Package q = demo();
    q.trips[5].passages[3].transfers.clear();
    EXPECT_TRUE(segnala(q, Check::NegativeOnBoard, 0, "corse.json", "/corse/5/passaggi/4"));
}

// Lo stesso controllo con due regolamenti diversi da' esiti diversi: l'attesa di 20 minuti prima delle lezioni
// va bene con R-TN (massimo 25) e non con R-INV (massimo 15).
TEST(Controlli, K_CTR_021_attesa_fuori_dal_regolamento) {
    EXPECT_EQ(quanti(demo(), Check::WaitOutsideRegulation), 0);
    Package p = demo();
    per_codice(p.contracts, "CT-SCU").regulation.code = "R-INV";
    EXPECT_TRUE(segnala(p, Check::WaitOutsideRegulation, 0, "scuole.json", "/scuole/0/orari/0/attesa_concordata"));
    EXPECT_TRUE(segnala(p, Check::WaitOutsideRegulation, 1, "scuole.json", "/scuole/0/orari/0/attesa_concordata"));
    EXPECT_EQ(quanti(p, Check::WaitOutsideRegulation), 2);
    // Con R-TN basta superare 25 minuti.
    Package q = demo();
    q.schools[0].timetables[0].wait->before_min = 26;
    EXPECT_TRUE(segnala(q, Check::WaitOutsideRegulation, 0, "scuole.json", "/scuole/0/orari/0/attesa_concordata"));
}

TEST(Controlli, K_CTR_022_mezzo_massimo_sotto_il_minimo) {
    Package p = demo();
    per_codice(p.contracts, "CT-LIN").rules.max_seats = 20;   // minimo 30
    EXPECT_TRUE(segnala(p, Check::MaxSeatsBelowMin, 0, "contratti.json", "/contratti/2/regole"));
    Package q = demo();
    per_codice(q.services, "S-SUP-RM").rules.min_seats = 60;   // massimo 54, dal contratto
    EXPECT_TRUE(segnala(q, Check::MaxSeatsBelowMin, 1, "servizi.json", "/servizi/2/regole"));
}

TEST(Controlli, K_CTR_023_trasbordo_non_autorizzato) {
    Package p = demo();
    per_codice(p.places, "L-TRASB-PONTE").transfer.reset();
    EXPECT_TRUE(segnala(p, Check::TransferNotAuthorized, 0, "corse.json", "/corse/4/passaggi/2/trasbordi/0"));
    EXPECT_TRUE(segnala(p, Check::TransferNotAuthorized, 0, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
    EXPECT_EQ(quanti(demo(), Check::TransferNotAuthorized), 0);   // a voce e' comunque autorizzato
}

TEST(Controlli, K_CTR_024_trasbordo_senza_la_sua_meta) {
    Package p = demo();
    p.trips[5].passages[3].transfers[0].code = "T-02";
    EXPECT_TRUE(segnala(p, Check::TransferHalfMissing, 0, "corse.json", "/corse/4/passaggi/2/trasbordi/0"));
    EXPECT_TRUE(segnala(p, Check::TransferHalfMissing, 1, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
}

// La corsa che riceve deve essere sul posto prima di quella che consegna, con il margine del manifesto (3 minuti).
TEST(Controlli, K_CTR_025_corsa_che_riceve_in_ritardo) {
    Package p = demo();
    p.trips[5].passages[3].time = ora("08:24");   // consegna alle 08:25: doveva arrivare entro le 08:22
    EXPECT_TRUE(segnala(p, Check::TransferLateReceiver, 0, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::TransferLateReceiver);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "limite")->number, ora("08:22"));
    EXPECT_EQ(parametro(*x, "margine")->number, 3);

    Package giusto = demo();
    giusto.trips[5].passages[3].time = ora("08:22");   // proprio al limite: va bene
    EXPECT_EQ(quanti(giusto, Check::TransferLateReceiver), 0);
    p.manifest.transfer_margin_min = 0;   // il margine e' un dato: senza margine le 08:24 vanno bene
    EXPECT_EQ(quanti(p, Check::TransferLateReceiver), 0);
}

TEST(Controlli, K_CTR_026_meta_che_non_corrispondono) {
    {   // due consegne
        Package p = demo();
        TransferHalf h = p.trips[4].passages[2].transfers[0];
        h.src = dove("corse.json", "/corse/4/passaggi/2/trasbordi/1");
        p.trips[4].passages[2].transfers.push_back(h);
        EXPECT_TRUE(segnala(p, Check::TransferHalvesDiffer, 0, "corse.json", "/corse/4/passaggi/2/trasbordi/1"));
    }
    {   // due ricezioni
        Package p = demo();
        TransferHalf h = p.trips[5].passages[3].transfers[0];
        h.src = dove("corse.json", "/corse/5/passaggi/3/trasbordi/1");
        p.trips[5].passages[3].transfers.push_back(h);
        EXPECT_TRUE(segnala(p, Check::TransferHalvesDiffer, 1, "corse.json", "/corse/5/passaggi/3/trasbordi/1"));
    }
    {   // consegna e ricezione sulla stessa corsa
        Package p = demo();
        TransferHalf h = p.trips[5].passages[3].transfers[0];
        h.src = dove("corse.json", "/corse/4/passaggi/2/trasbordi/1");
        p.trips[4].passages[2].transfers.push_back(h);
        p.trips[5].passages[3].transfers.clear();
        EXPECT_TRUE(segnala(p, Check::TransferHalvesDiffer, 2, "corse.json", "/corse/4/passaggi/2/trasbordi/1"));
    }
    {   // luoghi diversi
        Package p = demo();
        p.trips[5].passages[3].place.code = "L-TRASB-CROCE";
        EXPECT_TRUE(segnala(p, Check::TransferHalvesDiffer, 3, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
    }
    {   // passeggero diverso
        Package p = demo();
        p.trips[5].passages[3].transfers[0].passenger.reset();
        EXPECT_TRUE(segnala(p, Check::TransferHalvesDiffer, 4, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
    }
}

TEST(Controlli, K_CTR_027_corsa_che_riceve_riparte_prima) {
    Package p = demo();
    p.trips[5].passages[3].departure = ora("08:22");   // la consegna arriva alle 08:25
    EXPECT_TRUE(segnala(p, Check::TransferReceiverLeaves, 0, "corse.json", "/corse/5/passaggi/3/trasbordi/0"));
}

TEST(Controlli, K_CTR_028_coincidenza_da_aspettare) {
    Package p = demo();
    p.trips[7].connections[0].time = ora("07:55");   // la corsa parte alle 07:52
    EXPECT_TRUE(segnala(p, Check::ConnectionWaitArrival, 0, "corse.json", "/corse/7/coincidenze/0"));
}

TEST(Controlli, K_CTR_029_coincidenza_da_garantire) {
    Package p = demo();
    p.trips[8].connections[0].time = ora("24:10");   // la corsa arriva alle 24:12
    EXPECT_TRUE(segnala(p, Check::ConnectionGuaranteeDeparture, 0, "corse.json", "/corse/8/coincidenze/0"));
}

TEST(Controlli, K_CTR_030_coincidenza_dove_la_corsa_non_passa) {
    Package p = demo();
    p.trips[7].connections[0].place.code = "F-SORGENTA";
    EXPECT_TRUE(segnala(p, Check::ConnectionNotOnTrip, 0, "corse.json", "/corse/7/coincidenze/0"));
}

TEST(Controlli, K_CTR_031_conteggi_diversi_dai_passeggeri) {
    Package p = demo();
    gruppo(p, "G-ASILO").stops[0].boarding = 5;   // a Sorgenta salgono 4 passeggeri (il sospeso non conta)
    EXPECT_TRUE(segnala(p, Check::CountsDifferFromPassengers, 0, "servizi.json", "/servizi/4/domanda/gruppi/0/fermate/0"));
    Package q = demo();
    gruppo(q, "G-ASILO").stops[2].alighting = 8;
    EXPECT_TRUE(segnala(q, Check::CountsDifferFromPassengers, 1, "servizi.json", "/servizi/4/domanda/gruppi/0/fermate/2"));
    Package r = demo();
    gruppo(r, "G-DIS").stops[0].wheelchairs = 0;   // P-D1 viaggia seduto sulla carrozzina
    EXPECT_TRUE(segnala(r, Check::CountsDifferFromPassengers, 2, "servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0"));
    Package s = demo();
    per_codice(gruppo(s, "G-ASILO").passengers, "P-A1").boarding->place.code = "F-PRADELLO";
    EXPECT_TRUE(segnala(s, Check::CountsDifferFromPassengers, 3, "servizi.json", "/servizi/4/domanda/gruppi/0/passeggeri/0"));
    Package t = demo();
    per_codice(gruppo(t, "G-ASILO").passengers, "P-A8").status = PassengerStatus::Active;   // non piu' sospeso
    EXPECT_TRUE(segnala(t, Check::CountsDifferFromPassengers, 0, "servizi.json", "/servizi/4/domanda/gruppi/0/fermate/1"));
}

// Carrozzine (persone sedute sulla carrozzina) e ausili piegati si confrontano con i passeggeri uno per uno.
TEST(Controlli, K_CTR_031_carrozzine_e_ausili_piegati) {
    EXPECT_EQ(quanti(demo(), Check::CountsDifferFromPassengers), 0);
    // P-D2 ha un deambulatore dichiarato: la fermata PP-02 deve riportare un ausilio piegato.
    Package p = demo();
    gruppo(p, "G-DIS").stops[1].folded_aids = 0;
    EXPECT_TRUE(segnala(p, Check::CountsDifferFromPassengers, 4, "servizi.json", "/servizi/3/domanda/gruppi/0/fermate/1"));
    EXPECT_EQ(quanti(p, Check::CountsDifferFromPassengers), 1);
    // P-D4 viaggia con la carrozzina piegata: e' un ausilio piegato, non una carrozzina.
    Package q = demo();
    gruppo(q, "G-DIS-GIO").stops[0].folded_aids = 0;
    EXPECT_TRUE(segnala(q, Check::CountsDifferFromPassengers, 4, "servizi.json", "/servizi/6/domanda/gruppi/0/fermate/0"));
    Package r = demo();
    gruppo(r, "G-DIS-GIO").stops[0].folded_aids = 0;
    gruppo(r, "G-DIS-GIO").stops[0].wheelchairs = 1;
    EXPECT_TRUE(segnala(r, Check::CountsDifferFromPassengers, 2, "servizi.json", "/servizi/6/domanda/gruppi/0/fermate/0"));
    EXPECT_TRUE(segnala(r, Check::CountsDifferFromPassengers, 4, "servizi.json", "/servizi/6/domanda/gruppi/0/fermate/0"));
    // Se P-D1 viaggiasse con la carrozzina piegata, a PP-01 cambierebbero tutti e due i conteggi.
    Package s = demo();
    per_codice(gruppo(s, "G-DIS").passengers, "P-D1").needs.wheelchair = WheelchairUse::Folded;
    EXPECT_TRUE(segnala(s, Check::CountsDifferFromPassengers, 2, "servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0"));
    EXPECT_TRUE(segnala(s, Check::CountsDifferFromPassengers, 4, "servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0"));
}

// Un passeggero che viaggia e ha bisogno dell'assistente, in un gruppo che non lo richiede: errore.
TEST(Controlli, K_CTR_047_assistente_non_richiesto_dal_gruppo) {
    EXPECT_EQ(quanti(demo(), Check::AssistantNotRequested), 0);
    Package p = demo();
    gruppo(p, "G-DIS").assistant_required = false;   // P-D1 ha bisogno dell'assistente
    EXPECT_TRUE(segnala(p, Check::AssistantNotRequested, 0, "servizi.json", "/servizi/3/domanda/gruppi/0/passeggeri/0"));
    EXPECT_EQ(quanti(p, Check::AssistantNotRequested), 1);
    EXPECT_EQ(api::definizione_controllo(Check::AssistantNotRequested).livello, api::Livello::Errore);
    const auto problemi = check_package(p);
    const Problem* x = primo(problemi, Check::AssistantNotRequested);
    ASSERT_NE(x, nullptr);
    ASSERT_NE(parametro(*x, "passeggero"), nullptr);
    EXPECT_EQ(parametro(*x, "passeggero")->text, "P-D1");
    ASSERT_NE(parametro(*x, "gruppo"), nullptr);
    EXPECT_EQ(parametro(*x, "gruppo")->text, "G-DIS");
    // Un passeggero sospeso non viaggia: nessun errore.
    per_codice(gruppo(p, "G-DIS").passengers, "P-D1").status = PassengerStatus::Suspended;
    EXPECT_EQ(quanti(p, Check::AssistantNotRequested), 0);
}

TEST(Controlli, K_CTR_032_servizio_di_un_altro_contratto_nel_lotto) {
    Package p = demo();
    p.lots[0].services[2].code = "S-ASILO";
    EXPECT_TRUE(segnala(p, Check::LotServiceOtherContract, 0, "gruppi_servizi.json", "/gruppi_servizi/0/servizi/2"));
}

TEST(Controlli, K_CTR_033_servizio_in_due_lotti) {
    Package p = demo();
    ServiceLot lotto;
    lotto.code = "LOTTO-3";
    lotto.contract = Ref{"CT-SCU", dove("gruppi_servizi.json", "/gruppi_servizi/1/contratto")};
    lotto.services = {Ref{"S-DIS", dove("gruppi_servizi.json", "/gruppi_servizi/1/servizi/0")}};
    lotto.src = dove("gruppi_servizi.json", "/gruppi_servizi/1");
    p.lots.push_back(lotto);
    EXPECT_TRUE(segnala(p, Check::ServiceInTwoLots, 0, "gruppi_servizi.json", "/gruppi_servizi/1/servizi/0"));
}

TEST(Controlli, K_CTR_034_gruppo_di_un_servizio_che_la_corsa_non_soddisfa) {
    Package p = demo();
    p.trips[2].services[0].code = "S-ASILO";   // C-AM-3 porta la primaria di S-SCU-AM
    EXPECT_TRUE(segnala(p, Check::GroupNotOnTripServices, 0, "corse.json", "/corse/2/passaggi/1/persone/0"));
}

// --- Avvisi ----------------------------------------------------------------------------------------------

TEST(Controlli, K_CTR_035_luogo_senza_coordinate) {
    EXPECT_TRUE(segnala(demo(), Check::PlaceWithoutCoordinates, 0, "luoghi.json", "/luoghi/5"));
    Package p = demo();
    per_codice(p.places, "L-MEDIE").coord.reset();
    EXPECT_TRUE(segnala(p, Check::PlaceWithoutCoordinates, 0, "luoghi.json", "/luoghi/11"));
}

TEST(Controlli, K_CTR_036_tempi_di_sosta_non_definiti) {
    EXPECT_TRUE(segnala(demo(), Check::DwellTimesUndefined, 0, "", ""));
    Package p = demo();
    p.dwell = DwellTimes{};
    p.dwell->src = dove("tempi_sosta.json", "/tempi");
    EXPECT_TRUE(segnala(p, Check::DwellTimesUndefined, 1, "tempi_sosta.json", "/tempi"));
    p.dwell->per_stop_s = 10;
    p.dwell->school_unload_s = 60;
    p.dwell->per_passenger = {{DwellKind::Student, 3, 3}, {DwellKind::Kindergarten, 8, 8}, {DwellKind::Disabled, 30, 30}};
    EXPECT_EQ(quanti(p, Check::DwellTimesUndefined), 0);
}

TEST(Controlli, K_CTR_037_durata_massima_non_definita) {
    EXPECT_EQ(quanti(demo(), Check::MaxRideUndefined), 0);
    Package p = demo();
    per_codice(p.contracts, "CT-LIN").regulation.code = "R-TN";   // R-TN non fissa la durata massima a bordo
    EXPECT_TRUE(segnala(p, Check::MaxRideUndefined, 0, "servizi.json", "/servizi/2/domanda/gruppi/0"));
}

TEST(Controlli, K_CTR_038_fermata_mai_usata) {
    Package p = demo();
    Place nuova = per_codice(p.places, "F-SORGENTA");
    nuova.code = "F-NUOVA";
    nuova.name = "Sorgenta, Fermata nuova";
    nuova.src = dove("luoghi.json", "/luoghi/18");
    p.places.push_back(nuova);
    EXPECT_TRUE(segnala(p, Check::StopNeverUsed, 0, "luoghi.json", "/luoghi/18"));
    EXPECT_EQ(quanti(p, Check::StopNeverUsed), 1);
}

TEST(Controlli, K_CTR_039_servizio_senza_corse) {
    Package p = demo();
    p.trips.erase(p.trips.begin() + 6);   // C-ASILO-1
    EXPECT_TRUE(segnala(p, Check::ServiceWithoutTrips, 0, "servizi.json", "/servizi/4"));
}

TEST(Controlli, K_CTR_040_corsa_senza_servizio) {
    Package p = demo();
    p.trips[0].services.clear();
    EXPECT_TRUE(segnala(p, Check::TripWithoutService, 0, "corse.json", "/corse/0"));
}

TEST(Controlli, K_CTR_041_gruppo_senza_passeggeri) {
    Package p = demo();
    for (DemandStop& d : gruppo(p, "G-RM-SUP").stops) d.boarding = d.alighting = 0;
    EXPECT_TRUE(segnala(p, Check::GroupWithoutPassengers, 0, "servizi.json", "/servizi/2/domanda/gruppi/0"));
}

// Es. domanda 8 a una fermata, corse 7.
TEST(Controlli, K_CTR_042_domanda_diversa_dalle_corse) {
    Package p = demo();
    gruppo(p, "G-AM-MEDIE").stops[2].boarding = 8;   // Montebrino: le corse ne caricano 7
    EXPECT_TRUE(segnala(p, Check::DemandDiffersFromTrips, 0, "servizi.json", "/servizi/0/domanda/gruppi/0/fermate/2"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::DemandDiffersFromTrips);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "domanda")->number, 8);
    EXPECT_EQ(parametro(*x, "corse")->number, 7);
    Package q = demo();
    gruppo(q, "G-AM-MEDIE").stops[3].alighting = 25;
    EXPECT_TRUE(segnala(q, Check::DemandDiffersFromTrips, 1, "servizi.json", "/servizi/0/domanda/gruppi/0/fermate/3"));
    Package r = demo();
    auto& fermate = gruppo(r, "G-AM-MEDIE").stops;
    fermate.erase(fermate.begin() + 2);   // Montebrino non e' piu' nella domanda
    EXPECT_TRUE(segnala(r, Check::DemandDiffersFromTrips, 2, "corse.json", "/corse/1/passaggi/2/persone/0"));
}

TEST(Controlli, K_CTR_043_trasbordo_autorizzato_a_voce) {
    EXPECT_TRUE(segnala(demo(), Check::TransferVerbalOnly, 0, "luoghi.json", "/luoghi/17/trasbordo"));
    Package p = demo();
    per_codice(p.places, "L-TRASB-CROCE").transfer->document = "Prot. 2026/0501";
    EXPECT_EQ(quanti(p, Check::TransferVerbalOnly), 0);
}

TEST(Controlli, K_CTR_044_mezzo_senza_costi) {
    Package p = demo();
    p.vehicles[0].daily_fixed_cost.reset();
    EXPECT_TRUE(segnala(p, Check::VehicleWithoutCosts, 0, "mezzi.json", "/mezzi/0"));
    p.vehicles[0].cost_per_km.reset();
    EXPECT_TRUE(segnala(p, Check::VehicleWithoutCosts, 2, "mezzi.json", "/mezzi/0"));
    Package q = demo();
    q.vehicles[1].cost_per_km.reset();
    EXPECT_TRUE(segnala(q, Check::VehicleWithoutCosts, 1, "mezzi.json", "/mezzi/1"));
}

TEST(Controlli, K_CTR_045_azienda_senza_mezzi) {
    Package p = demo();
    per_codice(p.vehicles, "BETA-01").company.code = "ALFA";
    per_codice(p.vehicles, "BETA-02").company.code = "ALFA";
    EXPECT_TRUE(segnala(p, Check::CompanyWithoutVehicles, 0, "pacchetto.json", "/aziende/1"));
}

TEST(Controlli, K_CTR_046_contratto_senza_regolamento_valido) {
    Package p = demo();
    per_codice(p.regulations, "R-INV").valid_to = giorno("2027-06-30");
    EXPECT_TRUE(segnala(p, Check::ContractWithoutRegulation, 0, "contratti.json", "/contratti/2/regolamento"));
    const auto problemi_p = check_package(p);
    const Problem* x = primo(problemi_p, Check::ContractWithoutRegulation);
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(parametro(*x, "dal")->number, giorno("2027-07-01"));
    EXPECT_EQ(parametro(*x, "al")->number, giorno("2027-08-31"));
    Package q = demo();
    per_codice(q.regulations, "R-TN").valid_from = giorno("2026-10-01");
    EXPECT_TRUE(segnala(q, Check::ContractWithoutRegulation, 0, "contratti.json", "/contratti/0/regolamento"));
    EXPECT_TRUE(segnala(q, Check::ContractWithoutRegulation, 0, "contratti.json", "/contratti/1/regolamento"));
}

// Ogni controllo sul contenuto ha la sua prova qui sopra (i controlli sulla forma dei file stanno in test_api).
TEST(Controlli, catalogo_coperto) {
    EXPECT_EQ(api::definizione_controllo(Check::DuplicateCode).codice, "K-CTR-009");
    EXPECT_EQ(api::definizione_controllo(Check::ContractWithoutRegulation).codice, "K-CTR-046");
    EXPECT_EQ(api::definizione_controllo(Check::AssistantNotRequested).codice, "K-CTR-047");
    EXPECT_EQ(api::definizione_controllo(Check::NotUtf8).codice, "K-CTR-048");
    EXPECT_EQ(api::definizione_controllo(Check::CalendarCombinationCycle).codice, "K-CTR-049");
    EXPECT_EQ(api::definizione_controllo(Check::CompanyWithoutNorms).codice, "K-CTR-052");
    EXPECT_EQ(api::definizione_controllo(Check::FileTooBig).codice, "K-CTR-053");
    EXPECT_EQ(api::definizione_controllo(Check::ShiftPartInvalid).codice, "K-CTR-054");
    EXPECT_EQ(api::definizione_controllo(Check::PersonOverlap).codice, "K-CTR-063");
    EXPECT_EQ(api::definizione_controllo(Check::DriverWithoutLicence).codice, "K-CTR-068");
    EXPECT_EQ(api::definizione_controllo(Check::ShiftUncovered).codice, "K-CTR-077");
    EXPECT_EQ(api::definizione_controllo(Check::EscortProviderDiffers).codice, "K-CTR-087");
    EXPECT_EQ(api::definizione_controllo(Check::NotRegularFile).codice, "K-CTR-088");   // K-009: lettura blindata
    EXPECT_EQ(api::definizione_controllo(Check::LinkOutsideFolder).codice, "K-CTR-089");
    EXPECT_EQ(api::definizione_controllo(Check::DirectoryInsteadOfFile).codice, "K-CTR-090");
    EXPECT_EQ(api::definizione_controllo(Check::VaultMissing).codice, "K-CTR-091");   // K-010: cassaforte
    EXPECT_EQ(api::definizione_controllo(Check::RetentionExpired).codice, "K-CTR-097");
    EXPECT_EQ(api::definizione_controllo(Check::KeysFromEnvelope).codice, "K-CTR-098");   // K-010 aggiunta 1: password
    EXPECT_EQ(api::definizione_controllo(Check::UserRemovedPasswords).codice, "K-CTR-099");
    EXPECT_EQ(api::definizione_controllo(Check::PasswordWrittenByOther).codice, "K-CTR-100");
    // K-011: orari di lavoro (le prove dei controlli stanno in test_orari, quelle del rientro in test_regole).
    EXPECT_EQ(api::definizione_controllo(Check::TimeCategoryMissing).codice, "K-CTR-101");
    EXPECT_EQ(api::definizione_controllo(Check::LawWorkViolated).codice, "K-CTR-110");
    EXPECT_EQ(api::definizione_controllo(Check::ReturnNotJudged).codice, "K-CTR-125");
    EXPECT_EQ(api::definizione_controllo(Check::WorkRuleIncoherent).codice, "K-CTR-126");
    // K-011 aggiunta 1: modo delle corse (OUT o tachigrafo).
    EXPECT_EQ(api::definizione_controllo(Check::ChosenRegimeLimit).codice, "K-CTR-127");
    EXPECT_EQ(api::definizione_controllo(Check::ModeForced).codice, "K-CTR-129");
    EXPECT_EQ(api::definizione_controllo(Check::ModeMixed).codice, "K-CTR-133");
    EXPECT_EQ(api::catalogo_controlli().size(), 133u);
}
