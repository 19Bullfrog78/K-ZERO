// K-Zero API - comando "giornata": i turni di un giorno del pacchetto (chi li fa e da dove viene l'assegnazione, con
// quale mezzo, corse e attivita' in ordine, tempi fermi, km), tutti oppure di una persona o di un mezzo, in testo o in
// JSON. Il documento JSON si costruisce gia' filtrato secondo le classi dei dati (vista.hpp); il testo si scrive da
// quel documento, cosi' il filtro e' lo stesso per le due uscite.
// Se il pacchetto ha errori nei dati la giornata non si mostra (risultati falsati sono peggio di nessun risultato):
// errore K-ERR-018, che rimanda a kzero controlla. Gli avvisi non bloccano.
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

#include "controllo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "turni.hpp"
#include "vista.hpp"

namespace kzero::api {

namespace {

// --- Etichette --------------------------------------------------------------------------------------------------

const char* nome_ruolo(kzero::ShiftRole r) {
    switch (r) {
        case kzero::ShiftRole::Driver: return "autista";
        case kzero::ShiftRole::Escort: return "assistente";
        case kzero::ShiftRole::SecondDriver: return "secondo_autista";
    }
    return "?";
}

// Giorno della settimana come nei file dei dati (senza apostrofo).
const char* chiave_giorno(kzero::Weekday g) {
    switch (g) {
        case kzero::Weekday::Monday: return "lunedi";
        case kzero::Weekday::Tuesday: return "martedi";
        case kzero::Weekday::Wednesday: return "mercoledi";
        case kzero::Weekday::Thursday: return "giovedi";
        case kzero::Weekday::Friday: return "venerdi";
        case kzero::Weekday::Saturday: return "sabato";
        case kzero::Weekday::Sunday: return "domenica";
    }
    return "?";
}

const char* modo_persona(kzero::PersonSource s) {
    switch (s) {
        case kzero::PersonSource::Rotation: return "rotazione";
        case kzero::PersonSource::Fixed: return "servizio_fisso";
        case kzero::PersonSource::Day: return "giorno";
        case kzero::PersonSource::None: break;
    }
    return "nessuno";
}

const char* modo_mezzo(kzero::VehicleSource s) {
    switch (s) {
        case kzero::VehicleSource::Shift: return "turno";
        case kzero::VehicleSource::Usual: return "abituale";
        case kzero::VehicleSource::Day: return "giorno";
        case kzero::VehicleSource::None: break;
    }
    return "nessuno";
}

Json posizione(const kzero::Source& s) {
    Json j = Json::object();
    j["file"] = s.file;
    j["puntatore"] = s.pointer;
    return j;
}

// --- Il documento JSON ------------------------------------------------------------------------------------------

class Giornata {
public:
    Giornata(const kzero::Planner& piano, const Vista& vista) : piano_(piano), idx_(piano.index()), vista_(vista) {}

    Json luogo(const kzero::Code& codice) const {
        Json j = Json::object();
        j["codice"] = codice;
        const kzero::Place* l = idx_.place(codice);
        j["nome"] = l ? vista_.campo(l->src, "nome", l->name, l->code) : Json(nullptr);
        return j;
    }

    Json calendario(const kzero::Code& codice) const {
        Json j = Json::object();
        j["codice"] = codice;
        const kzero::Calendar* c = idx_.calendar(codice);
        j["nome"] = c && !c->name.empty() ? Json(c->name) : Json(nullptr);
        j["giorni_nel_periodo"] = piano_.days().count(codice);
        return j;
    }

    Json persona(const kzero::Person& x) const {
        Json j = Json::object();
        j["codice"] = x.code;
        j["nome"] = x.name.empty() ? Json(nullptr) : vista_.campo(x.src, "nome", x.name, x.code);
        j["azienda"] = x.company.code;
        j["ipotetica"] = x.hypothetical;
        return j;
    }

    static Json mezzo(const kzero::Vehicle& v) {
        Json j = Json::object();
        j["codice"] = v.code;
        j["azienda"] = v.company.code;
        j["targa"] = v.plate;
        j["categoria"] = nome_categoria_mezzo(v.category);
        j["posti"] = v.seats;
        j["posti_in_piedi"] = v.standing;
        j["uso"] = v.use ? Json(v.use->code) : Json(nullptr);
        j["scorta"] = v.spare;
        j["ipotetico"] = v.hypothetical;
        return j;
    }

    // "P-A", oppure "P-A (Rossi Mario)" con i dati personali.
    std::string rif_persona(const kzero::Person& x) const {
        if (x.name.empty() || !vista_.visibilita().personali) return x.code;
        return x.code + " (" + x.name + ")";
    }
    static std::string rif_mezzo(const kzero::Vehicle& v) { return v.code + " (" + v.plate + ")"; }

    // Perche' la persona prevista non fa il turno quel giorno. I fatti sulle persone (assenze, disponibilita',
    // abilitazioni, autorizzazioni) sono dati personali: di base il motivo non si dice.
    std::string motivo_persona(const kzero::ShiftDay& sd) const {
        if (!sd.planned) return "nessuna rotazione, servizio fisso o assegnazione del singolo giorno lo assegna a una persona.";
        const std::string chi = "la persona prevista " + rif_persona(*sd.planned);
        const bool personali = vista_.visibilita().personali;
        switch (sd.person_impediment) {
            case kzero::PersonImpediment::LoanNotValid:
                return "il prestito della persona prevista " + rif_persona(*sd.planned) + " all'azienda " + sd.shift->company.code +
                       " non vale quel giorno. Rimedio: un'assegnazione del singolo giorno.";
            case kzero::PersonImpediment::Absent:
                if (personali) return chi + " e' assente (" + sd.absence->type.code + "). Rimedio: un'assegnazione del singolo giorno.";
                break;
            case kzero::PersonImpediment::OutsideAvailability:
                if (personali) return chi + " non e' disponibile in quegli orari. Rimedio: un'assegnazione del singolo giorno.";
                break;
            case kzero::PersonImpediment::LicenceExpired:
                if (personali)
                    return "l'abilitazione " + sd.licence->type.code + " della persona prevista " + rif_persona(*sd.planned) +
                           " e' scaduta. Rimedio: un'assegnazione del singolo giorno.";
                break;
            case kzero::PersonImpediment::AuthorizationExpired:
                if (personali)
                    return "l'autorizzazione del committente " + sd.authorization->client.code + " della persona prevista " +
                           rif_persona(*sd.planned) + " e' scaduta. Rimedio: un'assegnazione del singolo giorno.";
                break;
            case kzero::PersonImpediment::None: return chi + " non fa il turno.";
        }
        return chi + " non puo' farlo (dati personali: riservati). Rimedio: un'assegnazione del singolo giorno.";
    }

    // Perche' un turno da autista che impegna un mezzo quel giorno non ne ha uno.
    std::string motivo_mezzo(const kzero::ShiftDay& sd) const {
        const kzero::Vehicle* v = sd.planned_vehicle;
        const std::string rimedio = " Rimedio: un'assegnazione del singolo giorno con un altro mezzo.";
        switch (sd.vehicle_impediment) {
            case kzero::VehicleImpediment::Stopped:
                return "il mezzo " + rif_mezzo(*v) + " e' in fermo (" + sd.stop->type.code + ")." + rimedio;
            case kzero::VehicleImpediment::UseNotAuthorized:
                return "l'autorizzazione del mezzo " + rif_mezzo(*v) + " all'uso " + sd.use_authorization->use.code +
                       " (distrazione) non vale quel giorno." + rimedio;
            case kzero::VehicleImpediment::LoanNotValid:
                return "il prestito del mezzo " + rif_mezzo(*v) + " all'azienda " + sd.shift->company.code + " non vale quel giorno." +
                       rimedio;
            case kzero::VehicleImpediment::None: break;
        }
        switch (sd.shift->vehicle_mode) {
            case kzero::ShiftVehicleMode::ToAssign:
                return "il mezzo e' da assegnare e nessuna assegnazione del singolo giorno gliene da' uno.";
            case kzero::ShiftVehicleMode::Usual:
                if (!sd.person) return "il turno usa il mezzo abituale di chi lo fa, ma e' scoperto.";
                return "il turno usa il mezzo abituale di chi lo fa, ma " + rif_persona(*sd.person) + " non ne ha uno.";
            default: break;
        }
        return "nessun mezzo.";
    }

    Json elemento(const kzero::PlannedItem& it, kzero::Minutes fermo_prima, const kzero::Shift& s) const {
        Json j = Json::object();
        if (!it.trip) {
            const kzero::ShiftActivity& a = it.item->activity;
            const kzero::CompanySettings* impostazioni = idx_.settings(s.company.code);
            const kzero::ActivityType* tipo =
                impostazioni ? kzero::CompanySettings::find_in(impostazioni->activities, a.type.code) : nullptr;
            j["tipo"] = "attivita";
            j["attivita"] = a.type.code;
            j["nome"] = tipo && !tipo->name.empty() ? Json(tipo->name) : Json(nullptr);
            j["luogo"] = luogo(a.place.code);
            j["verso"] = a.to_place ? luogo(a.to_place->code) : Json(nullptr);
            j["inizio"] = orario(it.start);
            j["fine"] = orario(it.end);
            j["fermo_prima_minuti"] = fermo_prima;
            j["km_vuoti"] = arrotonda(it.empty_km, 3);
            j["usa_il_mezzo"] = it.uses_vehicle;
            j["nota"] = a.note.empty() ? Json(nullptr) : Json(a.note);
            return j;
        }
        const kzero::Trip& t = *it.trip;
        j["tipo"] = "corsa";
        j["corsa"] = t.code;
        j["nome"] = t.name.empty() ? Json(nullptr) : Json(t.name);
        j["intera"] = it.whole;
        j["inizio"] = orario(it.start);
        j["fine"] = orario(it.end);
        j["fermo_prima_minuti"] = fermo_prima;
        j["km_pieni"] = arrotonda(it.loaded_km, 3);
        j["km_vuoti"] = arrotonda(it.empty_km, 3);
        Json servizi = Json::array();
        bool non_noti = false;
        const kzero::Contract* primo_contratto = nullptr;
        for (const kzero::Ref& r : t.services) {
            const kzero::Service* sv = idx_.service(r.code);
            Json x = Json::object();
            x["codice"] = r.code;
            x["nome"] = sv && !sv->name.empty() ? Json(sv->name) : Json(nullptr);
            x["tipo"] = sv ? vista_.campo(sv->src, "tipo", nome_tipo_servizio(sv->kind), sv->code) : Json(nullptr);
            const kzero::Contract* c = sv ? idx_.contract(sv->contract.code) : nullptr;
            if (c && !primo_contratto) primo_contratto = c;
            x["contratto"] = sv ? Json(sv->contract.code) : Json(nullptr);
            x["committente"] = c ? Json(c->client.code) : Json(nullptr);
            servizi.push_back(x);
            if (sv)
                for (const auto& g : sv->groups) non_noti = non_noti || g.counts == kzero::CountKind::Unknown;
        }
        j["servizi"] = servizi;
        j["calendario"] = calendario(t.calendar.code);
        // Chi fornisce l'assistente a bordo: il turno, se lo dice; altrimenti il contratto, se a bordo c'e' l'assistente.
        bool a_bordo = it.item->trip.escort.has_value();
        for (const kzero::Passage& ps : t.passages) a_bordo = a_bordo || ps.assistant != kzero::AssistantMove::None;
        const auto fornitore = [](kzero::EscortProvider f) { return f == kzero::EscortProvider::Company ? "azienda" : "committente"; };
        if (it.item->trip.escort) j["assistente"] = fornitore(*it.item->trip.escort);
        else if (a_bordo && primo_contratto) j["assistente"] = fornitore(primo_contratto->escort_provider);
        else j["assistente"] = nullptr;
        const std::vector<kzero::OnBoardAfter> bordo = kzero::on_board(t, idx_);
        Json passaggi = Json::array();
        for (std::size_t k = it.from; k <= it.to && k < t.passages.size(); ++k) {
            const kzero::Passage& ps = t.passages[k];
            Json y = Json::object();
            y["orario"] = orario(ps.time);
            y["partenza"] = ps.departure ? Json(orario(*ps.departure)) : Json(nullptr);
            const Json l = luogo(ps.place.code);
            y["luogo"] = l.at("codice");
            y["nome_luogo"] = l.at("nome");
            y["transito"] = ps.transit;
            int salgono = 0, scendono = 0;
            for (const auto& c : ps.persons) {
                salgono += c.boarding;
                scendono += c.alighting;
            }
            y["salgono"] = salgono;
            y["scendono"] = scendono;
            // I km di un passaggio sono quelli dal passaggio precedente: al primo passaggio del pezzo non contano.
            y["km_pieni"] = k == it.from ? 0.0 : arrotonda(ps.loaded_km.value_or(0.0), 3);
            y["km_vuoti"] = k == it.from ? 0.0 : arrotonda(ps.empty_km.value_or(0.0), 3);
            y["assistente"] = ps.assistant == kzero::AssistantMove::Boards    ? Json("sale")
                              : ps.assistant == kzero::AssistantMove::Alights ? Json("scende")
                                                                              : Json(nullptr);
            Json trasbordi = Json::array();
            for (const auto& h : ps.transfers)
                trasbordi.push_back({{"codice", h.code},
                                     {"azione", h.action == kzero::TransferAction::Deliver ? "consegna" : "ricezione"},
                                     {"persone", h.persons}});
            y["trasbordi"] = trasbordi;
            y["a_bordo"] = non_noti ? Json(nullptr) : Json(bordo[k].total);
            passaggi.push_back(y);
        }
        j["passaggi"] = passaggi;
        return j;
    }

    Json turno(const kzero::ShiftDay& sd) const {
        const kzero::Shift& s = *sd.shift;
        Json j = Json::object();
        j["codice"] = s.code;
        j["nome"] = s.name.empty() ? Json(nullptr) : Json(s.name);
        j["azienda"] = s.company.code;
        j["ruolo"] = nome_ruolo(s.role);
        j["calendario"] = calendario(s.calendar.code);
        j["giorni_nel_periodo"] = piano_.shift_days(s);
        j["inizio"] = orario(sd.start);
        j["fine"] = orario(sd.end);
        j["durata_minuti"] = sd.end - sd.start;
        j["luogo_inizio"] = luogo(s.start_place.code);
        j["luogo_fine"] = luogo(s.end_place.code);

        j["persona"] = sd.person ? persona(*sd.person) : Json(nullptr);
        Json assegnazione = Json::object();
        assegnazione["modo"] = modo_persona(sd.person_source);
        if (sd.person_source == kzero::PersonSource::Rotation) {
            assegnazione["rotazione"] = sd.rotation.rotation->code;
            assegnazione["passo"] = sd.rotation.step + 1;
        }
        if (sd.person_source == kzero::PersonSource::Fixed) assegnazione["servizio_fisso"] = sd.fixed->code;
        assegnazione["posizione"] = sd.person_source == kzero::PersonSource::None ? Json(nullptr) : posizione(sd.person_position());
        j["assegnazione"] = assegnazione;
        j["persona_prevista"] = sd.planned && sd.planned != sd.person ? persona(*sd.planned) : Json(nullptr);
        j["scoperto"] = sd.uncovered();
        j["motivo_scoperto"] = sd.uncovered() ? Json(motivo_persona(sd)) : Json(nullptr);

        const bool impegna_il_mezzo = s.role == kzero::ShiftRole::Driver && s.vehicle_mode != kzero::ShiftVehicleMode::None &&
                                      sd.vehicle_start.has_value();
        j["mezzo"] = sd.vehicle ? mezzo(*sd.vehicle) : Json(nullptr);
        j["mezzo_da"] = modo_mezzo(sd.vehicle_source);
        // Il mezzo scritto nel turno, quando quel giorno un'assegnazione del singolo giorno ne da' un altro.
        j["mezzo_del_turno"] = sd.vehicle_source == kzero::VehicleSource::Day && s.vehicle_mode == kzero::ShiftVehicleMode::Specific &&
                                       s.vehicle && (!sd.vehicle || sd.vehicle->code != s.vehicle->code)
                                   ? Json(s.vehicle->code)
                                   : Json(nullptr);
        j["posizione_mezzo"] = sd.vehicle_source == kzero::VehicleSource::Day ? posizione(sd.vehicle_position()) : Json(nullptr);
        j["mezzo_previsto"] = sd.planned_vehicle && sd.planned_vehicle != sd.vehicle ? mezzo(*sd.planned_vehicle) : Json(nullptr);
        const bool senza_mezzo = impegna_il_mezzo && !sd.vehicle;
        j["senza_mezzo"] = senza_mezzo;
        j["motivo_senza_mezzo"] = senza_mezzo ? Json(motivo_mezzo(sd)) : Json(nullptr);
        j["a_bordo_di"] = sd.aboard ? mezzo(*sd.aboard) : Json(nullptr);
        Json competenze = Json::array();
        for (const kzero::Ref& c : s.competences) competenze.push_back(c.code);
        j["competenze_richieste"] = s.competences.empty() ? competenze : vista_.campo(s.src, "competenze_richieste", competenze, s.code);
        j["nota"] = s.note.empty() ? Json(nullptr) : Json(s.note);

        Json elementi = Json::array();
        for (std::size_t i = 0; i < sd.items.size(); ++i) {
            const kzero::Minutes fermo = i == 0 ? 0 : std::max<kzero::Minutes>(0, sd.items[i].start - sd.items[i - 1].end);
            elementi.push_back(elemento(sd.items[i], fermo, s));
        }
        j["elementi"] = elementi;
        Json totali = Json::object();
        totali["km_pieni"] = arrotonda(kzero::loaded_km(sd), 3);
        totali["km_vuoti"] = arrotonda(kzero::empty_km(sd), 3);
        totali["minuti_fermi"] = kzero::idle_minutes(sd);
        j["totali"] = totali;
        return j;
    }

private:
    const kzero::Planner& piano_;
    const kzero::Index& idx_;
    const Vista& vista_;
};

// Dettaglio di una persona (assenze) da mostrare solo con i dati personali.
Json personale(const Vista& vista, Json valore) { return vista.visibilita().personali ? valore : Json(kRiservato); }

Json in_json(const DefinizioneComando& comando, const Resoconto& r, const Parametri& par, const Vista& vista) {
    const kzero::Package& p = r.lettura.pacchetto;
    const kzero::Planner piano(p);
    const kzero::Index& idx = piano.index();
    const Giornata g(piano, vista);
    const kzero::Day d = *par.data;

    Json j = documento(comando.schema);
    j["dati_inventati"] = p.manifest.invented;
    j["dati_personali_inclusi"] = vista.visibilita().personali;
    j["dati_particolari_inclusi"] = vista.visibilita().particolari;
    Json pacchetto = Json::object();
    pacchetto["codice"] = p.manifest.code;
    pacchetto["nome"] = p.manifest.name;
    pacchetto["livello"] = p.manifest.level == kzero::Level::Full ? "completo" : "solo_numeri";
    pacchetto["periodo"] = {{"dal", kzero::text_from_day(p.manifest.period.from)}, {"al", kzero::text_from_day(p.manifest.period.to)}};
    j["pacchetto"] = pacchetto;
    j["controllo"] = {{"errori", r.errori}, {"avvisi", r.avvisi}};
    j["data"] = kzero::text_from_day(d);
    j["giorno_della_settimana"] = chiave_giorno(kzero::weekday_of(d));
    Json festivita = Json::array();
    for (const kzero::HolidayCalendar& h : p.holidays)
        for (const kzero::Holiday& f : h.days)
            if (f.day == d)
                festivita.push_back({{"calendario", h.code}, {"nome", f.name}, {"da_confermare", f.to_confirm}});
    j["festivita"] = festivita;

    const kzero::Person* sola_persona = par.persona.empty() ? nullptr : idx.person(par.persona);
    const kzero::Vehicle* solo_mezzo = par.mezzo.empty() ? nullptr : idx.vehicle(par.mezzo);
    Json vista_json = Json::object();
    vista_json["tipo"] = sola_persona ? "persona" : solo_mezzo ? "mezzo" : "tutti";
    vista_json["codice"] = sola_persona ? Json(sola_persona->code) : solo_mezzo ? Json(solo_mezzo->code) : Json(nullptr);
    j["vista"] = vista_json;
    if (sola_persona) {
        Json x = g.persona(*sola_persona);
        x["presa_servizio"] = sola_persona->base_place ? g.luogo(sola_persona->base_place->code) : Json(nullptr);
        x["mezzo_abituale"] = sola_persona->usual_vehicle ? Json(sola_persona->usual_vehicle->code) : Json(nullptr);
        Json assenze = Json::array();
        for (const kzero::Absence& a : p.absences)
            if (a.person.code == sola_persona->code && d >= a.days.from && d <= a.days.to)
                assenze.push_back({{"tipo", personale(vista, a.type.code)},
                                   {"dalle", a.start ? personale(vista, orario(*a.start)) : Json(nullptr)},
                                   {"alle", a.end ? personale(vista, orario(*a.end)) : Json(nullptr)}});
        x["assenze"] = assenze;
        j["persona"] = x;
    }
    if (solo_mezzo) {
        Json x = Giornata::mezzo(*solo_mezzo);
        Json fermi = Json::array();
        for (const kzero::VehicleStop& f : p.stops)
            if (f.vehicle.code == solo_mezzo->code && d >= f.days.from && d <= f.days.to)
                fermi.push_back({{"tipo", f.type.code},
                                 {"dalle", f.start ? Json(orario(*f.start)) : Json(nullptr)},
                                 {"alle", f.end ? Json(orario(*f.end)) : Json(nullptr)}});
        x["fermi"] = fermi;
        j["mezzo"] = x;
    }

    Json turni = Json::array();
    int scoperti = 0, senza_mezzo = 0;
    long long fermi = 0;
    double pieni = 0.0, vuoti = 0.0;
    std::set<kzero::Code> persone, mezzi;
    for (const kzero::ShiftDay& sd : piano.day(d)) {
        const auto e = [](const auto* x, const auto* y) { return x && y && x->code == y->code; };
        const bool scritto = solo_mezzo && sd.shift->vehicle_mode == kzero::ShiftVehicleMode::Specific && sd.shift->vehicle &&
                             sd.shift->vehicle->code == solo_mezzo->code;   // il mezzo scritto nel turno
        if (sola_persona && !e(sd.person, sola_persona) && !e(sd.planned, sola_persona)) continue;
        if (solo_mezzo && !e(sd.vehicle, solo_mezzo) && !e(sd.planned_vehicle, solo_mezzo) && !e(sd.aboard, solo_mezzo) && !scritto)
            continue;
        Json t = g.turno(sd);
        scoperti += t.at("scoperto").get<bool>() ? 1 : 0;
        senza_mezzo += t.at("senza_mezzo").get<bool>() ? 1 : 0;
        fermi += kzero::idle_minutes(sd);
        // I km sono quelli dei mezzi guidati: della persona, del mezzo o di tutti i turni da autista del giorno.
        const bool guidato = sd.shift->role == kzero::ShiftRole::Driver && (!sola_persona || e(sd.person, sola_persona)) &&
                             (!solo_mezzo || e(sd.vehicle, solo_mezzo));
        if (guidato) {
            pieni += kzero::loaded_km(sd);
            vuoti += kzero::empty_km(sd);
        }
        if (sd.person) persone.insert(sd.person->code);
        if (sd.vehicle) mezzi.insert(sd.vehicle->code);
        turni.push_back(t);
    }
    Json totali = Json::object();
    totali["turni"] = turni.size();
    totali["scoperti"] = scoperti;
    totali["senza_mezzo"] = senza_mezzo;
    totali["persone"] = persone.size();
    totali["mezzi"] = mezzi.size();
    totali["km_pieni"] = arrotonda(pieni, 3);
    totali["km_vuoti"] = arrotonda(vuoti, 3);
    totali["minuti_fermi"] = fermi;
    j["turni"] = turni;
    j["totali"] = totali;
    return j;
}

// --- Il testo, scritto dal documento JSON -----------------------------------------------------------------------

// "L-RIMESSA-ALFA (Valfonda, rimessa Autoservizi Alfa)": il nome solo se si vede e non e' il codice.
std::string luogo_testo(const Json& l) {
    const std::string codice = l.at("codice").get<std::string>();
    const Json& nome = l.at("nome");
    if (nome.is_null() || nome == l.at("codice")) return codice;
    return codice + " (" + nome.get<std::string>() + ")";
}

std::string persona_testo(const Json& x) {
    std::string t = x.at("codice").get<std::string>();
    if (!x.at("nome").is_null() && x.at("nome") != x.at("codice")) t += " " + x.at("nome").get<std::string>();
    if (x.at("ipotetica").get<bool>()) t += " (ipotetica)";
    return t;
}

std::string mezzo_testo(const Json& v) {
    std::string t = v.at("codice").get<std::string>() + " (" + v.at("targa").get<std::string>() + ", " +
                    v.at("categoria").get<std::string>() + ", " + testo(v.at("posti")) + " posti";
    if (v.at("posti_in_piedi").get<int>() > 0) t += " + " + testo(v.at("posti_in_piedi")) + " in piedi";
    if (!v.at("uso").is_null()) t += ", uso " + v.at("uso").get<std::string>();
    t += ")";
    if (v.at("scorta").get<bool>()) t += " (di scorta)";
    if (v.at("ipotetico").get<bool>()) t += " (ipotetico)";
    return t;
}

std::string calendario_testo(const Json& c) {
    std::string t = c.at("codice").get<std::string>();
    if (!c.at("nome").is_null()) t += " \"" + c.at("nome").get<std::string>() + "\"";
    return t;
}

// "nessuno scoperto", "1 scoperto", "2 scoperti".
std::string quanti_turni(long long n, const char* uno, const char* molti) {
    return n == 0 ? std::string("nessuno ") + uno : std::to_string(n) + " " + (n == 1 ? uno : molti);
}

std::string posizione_testo(const Json& p) {
    return p.is_null() ? std::string() : " (" + p.at("file").get<std::string>() + " " + p.at("puntatore").get<std::string>() + ")";
}

std::string km_testo(const Json& pieni, const Json& vuoti) {
    return "km a pieno " + testo(pieni) + ", a vuoto " + testo(vuoti);
}

void elemento_testo(std::string& t, const Json& e) {
    const long long fermo = e.at("fermo_prima_minuti").get<long long>();
    if (fermo > 0) t += "                (fermo " + durata(fermo) + ")\n";
    const std::string orari = "   " + e.at("inizio").get<std::string>() + "-" + e.at("fine").get<std::string>() + "  ";
    if (e.at("tipo") == "attivita") {
        t += orari + (e.at("nome").is_null() ? std::string("attivita'") : e.at("nome").get<std::string>()) + " (" +
             e.at("attivita").get<std::string>() + ")";
        if (e.at("verso").is_null()) t += ", a " + luogo_testo(e.at("luogo"));
        else t += ", da " + luogo_testo(e.at("luogo")) + " a " + luogo_testo(e.at("verso"));
        if (e.at("km_vuoti").get<double>() > 0) t += ", km a vuoto " + testo(e.at("km_vuoti"));
        if (!e.at("usa_il_mezzo").get<bool>()) t += ", senza il mezzo";
        if (!e.at("nota").is_null()) t += ". Nota: " + e.at("nota").get<std::string>();
        t += "\n";
        return;
    }
    t += orari + "corsa " + e.at("corsa").get<std::string>();
    if (!e.at("nome").is_null()) t += " \"" + e.at("nome").get<std::string>() + "\"";
    if (!e.at("intera").get<bool>()) t += ", solo un pezzo (cambio dell'autista)";
    t += "\n";
    std::string servizi;
    for (const auto& s : e.at("servizi")) {
        servizi += (servizi.empty() ? "" : "; ") + s.at("codice").get<std::string>() + " (tipo " + testo(s.at("tipo")) +
                   ", contratto " + testo(s.at("contratto")) + ", committente " + testo(s.at("committente")) + ")";
    }
    const Json& cal = e.at("calendario");
    t += "                servizi " + servizi + "\n";
    t += "                calendario " + calendario_testo(cal) + ", " + testo(cal.at("giorni_nel_periodo")) + " giorni nel periodo; " +
         km_testo(e.at("km_pieni"), e.at("km_vuoti"));
    if (!e.at("assistente").is_null()) t += "; assistente fornito da" + std::string(e.at("assistente") == "azienda" ? "ll'azienda" : "l committente");
    t += "\n";
    for (const auto& ps : e.at("passaggi")) {
        t += "      " + ps.at("orario").get<std::string>() + "  " + ps.at("luogo").get<std::string>();
        if (!ps.at("nome_luogo").is_null() && ps.at("nome_luogo") != ps.at("luogo")) t += "  " + ps.at("nome_luogo").get<std::string>();
        if (!ps.at("partenza").is_null()) t += "  riparte " + ps.at("partenza").get<std::string>();
        if (ps.at("transito").get<bool>()) t += "  transito";
        if (ps.at("salgono").get<int>() > 0) t += "  salgono " + testo(ps.at("salgono"));
        if (ps.at("scendono").get<int>() > 0) t += "  scendono " + testo(ps.at("scendono"));
        if (!ps.at("assistente").is_null()) t += "  " + ps.at("assistente").get<std::string>() + " l'assistente";
        for (const auto& h : ps.at("trasbordi"))
            t += std::string("  ") + (h.at("azione") == "consegna" ? "consegna" : "riceve") + " il trasbordo " +
                 h.at("codice").get<std::string>() + " (" + testo(h.at("persone")) + ")";
        if (ps.at("km_pieni").get<double>() > 0) t += "  km a pieno " + testo(ps.at("km_pieni"));
        if (ps.at("km_vuoti").get<double>() > 0) t += "  km a vuoto " + testo(ps.at("km_vuoti"));
        t += "  a bordo " + (ps.at("a_bordo").is_null() ? std::string("non noto") : testo(ps.at("a_bordo"))) + "\n";
    }
}

std::string assegnazione_testo(const Json& a) {
    const std::string modo = a.at("modo").get<std::string>();
    if (modo == "rotazione")
        return "rotazione " + a.at("rotazione").get<std::string>() + ", passo " + testo(a.at("passo")) + posizione_testo(a.at("posizione"));
    if (modo == "servizio_fisso") return "servizio fisso " + a.at("servizio_fisso").get<std::string>() + posizione_testo(a.at("posizione"));
    if (modo == "giorno") return "assegnazione del singolo giorno" + posizione_testo(a.at("posizione"));
    return "nessuna";
}

void turno_testo(std::string& t, const Json& x) {
    t += "\n" + x.at("codice").get<std::string>();
    if (!x.at("nome").is_null()) t += " \"" + x.at("nome").get<std::string>() + "\"";
    t += "  " + x.at("inizio").get<std::string>() + "-" + x.at("fine").get<std::string>() + " (" +
         durata(x.at("durata_minuti").get<long long>()) + ")  azienda " + x.at("azienda").get<std::string>() + ", " +
         testo(x.at("ruolo")) + "\n";
    if (x.at("scoperto").get<bool>()) {
        t += "   Persona: nessuna - TURNO SCOPERTO: " + x.at("motivo_scoperto").get<std::string>() + "\n";
        if (!x.at("persona_prevista").is_null())
            t += "   Prevista: " + persona_testo(x.at("persona_prevista")) + ", " + assegnazione_testo(x.at("assegnazione")) + "\n";
    } else {
        t += "   Persona: " + persona_testo(x.at("persona")) + ", " + assegnazione_testo(x.at("assegnazione")) + "\n";
    }
    const std::string da = x.at("mezzo_da").get<std::string>();
    if (!x.at("mezzo").is_null()) {
        t += "   Mezzo: " + mezzo_testo(x.at("mezzo")) + ", " +
             (da == "giorno"     ? "assegnazione del singolo giorno" + posizione_testo(x.at("posizione_mezzo")) +
                                   (x.at("mezzo_del_turno").is_null()
                                        ? std::string()
                                        : ", al posto di " + x.at("mezzo_del_turno").get<std::string>() + " scritto nel turno")
              : da == "abituale" ? std::string("mezzo abituale della persona")
                                 : std::string("scritto nel turno")) +
             "\n";
    } else if (x.at("senza_mezzo").get<bool>()) {
        t += "   Mezzo: nessuno - TURNO SENZA MEZZO: " + x.at("motivo_senza_mezzo").get<std::string>() + "\n";
    } else if (!x.at("a_bordo_di").is_null()) {
        t += "   A bordo del mezzo: " + mezzo_testo(x.at("a_bordo_di")) + " (il mezzo del turno dell'autista)\n";
    } else {
        t += "   Mezzo: nessuno (il turno non usa un mezzo)\n";
    }
    const Json& cal = x.at("calendario");
    t += "   Calendario " + calendario_testo(cal) + "; il turno c'e' in " + testo(x.at("giorni_nel_periodo")) + " giorni del periodo\n";
    const Json& comp = x.at("competenze_richieste");
    if (!comp.is_array() || !comp.empty()) {
        std::string c;
        if (comp.is_array())
            for (const auto& k : comp) c += (c.empty() ? "" : ", ") + k.get<std::string>();
        else
            c = testo(comp);
        t += "   Competenze richieste: " + c + "\n";
    }
    t += "   Comincia a " + luogo_testo(x.at("luogo_inizio")) + ", finisce a " + luogo_testo(x.at("luogo_fine")) + "\n";
    if (!x.at("nota").is_null()) t += "   Nota: " + x.at("nota").get<std::string>() + "\n";
    for (const auto& e : x.at("elementi")) elemento_testo(t, e);
    const Json& tot = x.at("totali");
    t += "   Totale del turno: " + km_testo(tot.at("km_pieni"), tot.at("km_vuoti")) + "; fermo " +
         durata(tot.at("minuti_fermi").get<long long>()) + ".\n";
}

std::string in_testo(const Json& j) {
    std::string t;
    const Json& pk = j.at("pacchetto");
    const std::string giorno = j.at("giorno_della_settimana").get<std::string>();
    const std::string apostrofo = giorno == "sabato" || giorno == "domenica" ? "" : "'";
    t += "Giornata del " + j.at("data").get<std::string>() + ", " + giorno + apostrofo + " - pacchetto " +
         pk.at("codice").get<std::string>() + " \"" + pk.at("nome").get<std::string>() + "\"" +
         (j.at("dati_inventati").get<bool>() ? " (dati inventati)" : "") + "\n";
    const Json& v = j.at("vista");
    if (v.at("tipo") == "persona") {
        const Json& x = j.at("persona");
        t += "Persona " + persona_testo(x) + ", azienda " + x.at("azienda").get<std::string>();
        if (!x.at("presa_servizio").is_null()) t += ", presa servizio a " + luogo_testo(x.at("presa_servizio"));
        if (!x.at("mezzo_abituale").is_null()) t += ", mezzo abituale " + x.at("mezzo_abituale").get<std::string>();
        t += "\n";
        for (const auto& a : x.at("assenze")) {
            t += "Assenza: " + testo(a.at("tipo"));
            if (!a.at("dalle").is_null()) t += ", dalle " + testo(a.at("dalle")) + " alle " + testo(a.at("alle"));
            else t += ", tutto il giorno";
            t += "\n";
        }
    } else if (v.at("tipo") == "mezzo") {
        const Json& x = j.at("mezzo");
        t += "Mezzo " + mezzo_testo(x) + ", azienda " + x.at("azienda").get<std::string>() + "\n";
        for (const auto& f : x.at("fermi")) {
            t += "Fermo: " + f.at("tipo").get<std::string>();
            if (!f.at("dalle").is_null()) t += ", dalle " + testo(f.at("dalle")) + " alle " + testo(f.at("alle"));
            else t += ", tutto il giorno";
            t += "\n";
        }
    } else {
        t += "Tutti i turni del giorno\n";
    }
    std::string feste;
    for (const auto& f : j.at("festivita"))
        feste += (feste.empty() ? "" : ", ") + f.at("nome").get<std::string>() + " (" + f.at("calendario").get<std::string>() +
                 (f.at("da_confermare").get<bool>() ? ", da confermare" : "") + ")";
    t += "Festivita': " + (feste.empty() ? std::string("nessuna") : feste) + ".\n";
    t += "Controllo del pacchetto: " + quanti(j.at("controllo").at("errori").get<long long>(), "errore", "errori") + ", " +
         quanti(j.at("controllo").at("avvisi").get<long long>(), "avviso", "avvisi") + " (dettagli con kzero controlla).\n";
    t += std::string("Dati personali: ") + (j.at("dati_personali_inclusi").get<bool>() ? "mostrati" : "sostituiti dal codice") +
         "; dati particolari: " + (j.at("dati_particolari_inclusi").get<bool>() ? "mostrati" : "riservati") + ".\n";
    if (j.at("turni").empty()) t += "\nNessun turno in questo giorno.\n";
    for (const auto& x : j.at("turni")) turno_testo(t, x);
    const Json& tot = j.at("totali");
    t += "\nTotali: " + quanti(tot.at("turni").get<long long>(), "turno", "turni") + " (" +
         quanti_turni(tot.at("scoperti").get<long long>(), "scoperto", "scoperti") + ", " +
         quanti_turni(tot.at("senza_mezzo").get<long long>(), "senza mezzo", "senza mezzo") + "), " +
         (tot.at("persone").get<long long>() == 0 ? std::string("nessuna persona")
                                                   : quanti(tot.at("persone").get<long long>(), "persona", "persone")) +
         ", " +
         quanti(tot.at("mezzi").get<long long>(), "mezzo", "mezzi") + "; " + km_testo(tot.at("km_pieni"), tot.at("km_vuoti")) +
         " (dei mezzi guidati" + std::string(v.at("tipo") == "persona" ? " dalla persona" : v.at("tipo") == "mezzo" ? ": solo questo mezzo" : "") +
         "); fermi " + durata(tot.at("minuti_fermi").get<long long>()) + ".\n";
    return t;
}

}  // namespace

Risultato esegui_giornata(const DefinizioneComando& comando, const Parametri& parametri) {
    const std::string& cartella = parametri.argomenti.at(0);
    std::error_code ec;
    if (cartella.empty() || !std::filesystem::is_directory(std::filesystem::path(cartella), ec))
        throw ErroreKZero(Errore::CartellaNonTrovata,
                          "Cartella del pacchetto non trovata: '" + cartella +
                              "'. Indicare la cartella che contiene il manifesto pacchetto.json.");
    const Visibilita visibilita{parametri.includi_dati_personali, parametri.includi_dati_particolari};
    const Resoconto r = controlla_pacchetto(CartellaSuDisco(cartella), visibilita);
    if (r.errori > 0)
        throw ErroreKZero(Errore::PacchettoConErrori,
                          "Il pacchetto ha " + quanti(r.errori, "errore", "errori") +
                              " nei dati: la giornata non si mostra, perche' sarebbe falsata. Correggere prima gli errori: "
                              "kzero controlla " + cartella);
    const kzero::Package& p = r.lettura.pacchetto;
    const kzero::Day d = *parametri.data;
    if (d < p.manifest.period.from || d > p.manifest.period.to)
        throw ErroreKZero(Errore::DataFuoriPeriodo, "La data " + kzero::text_from_day(d) + " e' fuori dal periodo del pacchetto " +
                                                        p.manifest.code + " (dal " + kzero::text_from_day(p.manifest.period.from) +
                                                        " al " + kzero::text_from_day(p.manifest.period.to) + ").");
    const kzero::Index idx(p);
    if (!parametri.persona.empty() && !idx.person(parametri.persona))
        throw ErroreKZero(Errore::ElementoNonTrovato, "La persona " + parametri.persona + " non c'e' nel pacchetto " + p.manifest.code +
                                                          " (persone.json).");
    if (!parametri.mezzo.empty() && !idx.vehicle(parametri.mezzo))
        throw ErroreKZero(Errore::ElementoNonTrovato, "Il mezzo " + parametri.mezzo + " non c'e' nel pacchetto " + p.manifest.code +
                                                          " (mezzi.json).");
    const Json j = in_json(comando, r, parametri, Vista(r.lettura, visibilita));
    return {parametri.formato == Formato::Json ? testo_json(j) : in_testo(j), 0};
}

}  // namespace kzero::api
