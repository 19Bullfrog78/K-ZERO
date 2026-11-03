// K-Zero API - strumenti comuni ai comandi che mostrano i dati di un pacchetto (vedi vista.hpp).
#include "vista.hpp"

#include <algorithm>
#include <locale>
#include <sstream>

namespace kzero::api {

Vista::Vista(const Lettura& lettura, Visibilita visibilita)
    : lettura_(lettura), visibilita_(visibilita), indice_(lettura.pacchetto), legame_(&indice_) {}

Classe Vista::classe(const kzero::Source& elemento, const std::string& campo) const {
    const auto v = lettura_.versioni.find(elemento.file);
    if (v == lettura_.versioni.end()) return Classe::Particolare;
    return SchemiDati::incorporati().classe(elemento.file.substr(0, elemento.file.rfind(".json")), v->second,
                                            elemento.pointer + "/" + campo);
}

Json Vista::campo(const kzero::Source& elemento, const std::string& campo, Json valore, const std::string& codice) const {
    const Classe c = classe(elemento, campo);
    if (c == Classe::Personale && !legame_.nessuno() && legame_.legato(trasportato(elemento, lettura_), codice))
        return visibile(Classe::Personale, visibilita_) && visibile(Classe::Particolare, visibilita_) ? valore : Json(codice);
    return filtra(c, std::move(valore), codice);
}

Json Vista::filtra(Classe c, Json valore, const std::string& codice) const {
    if (visibile(c, visibilita_)) return valore;
    return c == Classe::Personale ? Json(codice) : Json(kRiservato);
}

Json Vista::modo_corsa(const kzero::TripMode& m) const {
    if (!m.any) return Json(nullptr);
    using F = kzero::TripMode::From;
    const Classe c = m.by_kinds || m.mixed_kinds ? Classe::Particolare : Classe::Comune;
    Json j = Json::object();
    // Il modo si vede (e' l'esito, come "vale" o "non vale" di un regime); da dove viene, se e' il tipo di servizio, no.
    j["modo"] = m.mode == kzero::TripModeKind::Out ? "out" : "tachigrafo";
    j["da"] = filtra(c, m.from == F::Trip ? "corsa" : m.from == F::Service ? "servizio" : m.from == F::Kind ? "tipo_di_servizio" : "impostazione", "");
    j["regime"] = m.modes ? Json(m.modes->regime.code) : Json(nullptr);
    j["forza"] = m.mode == kzero::TripModeKind::Tachograph ? Json(m.strength == kzero::ChoiceStrength::Limit ? "limite" : "preferenza") : Json(nullptr);
    // Motivo e chi: personali. Nella dichiarazione (dato particolare, K-011 aggiunta 2) stanno dentro un dato particolare:
    // si vedono solo con tutti e due i permessi, come nome e casa di chi va in un luogo sensibile. Un testo vuoto: non
    // arrivato dalla cassaforte (permessi), quindi riservato.
    const bool legati = visibile(Classe::Personale, visibilita_) && visibile(Classe::Particolare, visibilita_);
    const auto giustificazione = [&](const kzero::ModeEntry* e, bool forzatura) -> Json {
        if (!e) return Json(nullptr);
        const kzero::ModeJustification& g = forzatura ? *e->forcing : *e->declaration;
        const bool vede = forzatura ? visibile(Classe::Personale, visibilita_) : legati;
        Json x = Json::object();
        x["motivo"] = vede && !g.reason.empty() ? Json(g.reason) : Json(kRiservato);
        x["chi"] = vede && !g.who.empty() ? Json(g.who) : Json(kRiservato);
        x["quando"] = kzero::text_from_day(g.when);
        x["fino_al"] = g.until ? Json(kzero::text_from_day(*g.until)) : Json(nullptr);
        return x;
    };
    j["forzatura"] = giustificazione(m.forcing, true);
    // La dichiarazione c'e' solo dove l'esclusione e' da confermare (dice il tipo di servizio): senza i dati particolari
    // "riservato" su ogni corsa, che ci sia o no, e la riga di testo non la nomina.
    j["dichiarazione"] = visibile(Classe::Particolare, visibilita_) ? giustificazione(m.declaration, false) : Json(kRiservato);
    return j;
}

std::string testo_modo_corsa(const Json& m) {
    const auto txt = [](const Json& v) { return v.is_string() ? v.get<std::string>() : v.dump(); };
    const std::string da = txt(m.at("da"));
    std::string x = m.at("modo") == "out" ? std::string("OUT") : std::string("tachigrafo");
    if (m.at("forza").is_string()) x += " come " + m.at("forza").get<std::string>();
    x += da == "corsa"              ? ", scelta per la corsa"
         : da == "servizio"         ? ", scelta per il servizio"
         : da == "tipo_di_servizio" ? ", scelta per il tipo di servizio"
         : da == "impostazione"     ? ", per l'impostazione dell'azienda sulle scelte diverse"
                                    : ", per un dato particolare";
    const auto giustificazione = [&](const char* campo, const char* nome) {
        const Json& g = m.at(campo);
        if (!g.is_object()) return;   // nessuna, oppure riservata (la dichiarazione senza i dati particolari)
        // Il motivo e' un testo libero: tra virgolette, perche' puo' finire con il suo punto.
        const std::string motivo = g.at("motivo") == kRiservato ? txt(g.at("motivo")) : "\"" + txt(g.at("motivo")) + "\"";
        x += std::string(", ") + nome + " (motivo: " + motivo + "; scritta da " + txt(g.at("chi")) + " il " +
             txt(g.at("quando")) + (g.at("fino_al").is_null() ? std::string() : "; vale fino al " + txt(g.at("fino_al"))) + ")";
    };
    giustificazione("forzatura", "forzatura dell'azienda");
    giustificazione("dichiarazione", "dichiarazione dell'azienda");
    return x + " (regime " + txt(m.at("regime")) + ")";
}

const char* nome_tipo_luogo(kzero::PlaceType t) {
    switch (t) {
        case kzero::PlaceType::Stop: return "fermata";
        case kzero::PlaceType::School: return "scuola";
        case kzero::PlaceType::Center: return "centro";
        case kzero::PlaceType::Depot: return "rimessa";
        case kzero::PlaceType::ExternalShelter: return "ricovero_esterno";
        case kzero::PlaceType::Parking: return "punto_sosta";
        case kzero::PlaceType::TransferPoint: return "punto_trasbordo";
        case kzero::PlaceType::DoorToDoor: return "porta_a_porta";
    }
    return "?";
}

const char* nome_categoria(kzero::ServiceType t) {
    switch (t) {
        case kzero::ServiceType::Line: return "linea";
        case kzero::ServiceType::Disability: return "disabili";
        case kzero::ServiceType::School: return "scolastico";
        case kzero::ServiceType::Hybrid: return "ibrido";
        case kzero::ServiceType::Other: return "altro";
    }
    return "?";
}

const char* nome_tipo_servizio(kzero::ServiceKind k) {
    switch (k) {
        case kzero::ServiceKind::Line: return "linea";
        case kzero::ServiceKind::School: return "scolastico";
        case kzero::ServiceKind::Disability: return "disabili";
        case kzero::ServiceKind::Other: return "altro";
    }
    return "?";
}

const char* nome_tipo_gruppo(kzero::GroupType t) {
    switch (t) {
        case kzero::GroupType::Students: return "studenti";
        case kzero::GroupType::Disabled: return "disabili";
        case kzero::GroupType::LineUsers: return "utenti_linea";
        case kzero::GroupType::AuthorizedAdults: return "adulti_autorizzati";
        case kzero::GroupType::Other: return "altro";
    }
    return "?";
}

const char* nome_categoria_mezzo(kzero::VehicleCategory c) {
    switch (c) {
        case kzero::VehicleCategory::M1: return "M1";
        case kzero::VehicleCategory::M2: return "M2";
        case kzero::VehicleCategory::M3: return "M3";
    }
    return "?";
}

const char* nome_grado(kzero::Grade g) {
    switch (g) {
        case kzero::Grade::Kindergarten: return "infanzia";
        case kzero::Grade::Primary: return "primaria";
        case kzero::Grade::LowerSecondary: return "secondaria_primo_grado";
        case kzero::Grade::UpperSecondary: return "secondaria_secondo_grado";
    }
    return "?";
}

const char* nome_stato(kzero::PassengerStatus s) {
    switch (s) {
        case kzero::PassengerStatus::Active: return "attivo";
        case kzero::PassengerStatus::Suspended: return "sospeso";
        case kzero::PassengerStatus::Unlocked: return "sbloccato";
    }
    return "?";
}

const char* nome_giorno(kzero::Weekday g) {
    switch (g) {
        case kzero::Weekday::Monday: return "lunedi'";
        case kzero::Weekday::Tuesday: return "martedi'";
        case kzero::Weekday::Wednesday: return "mercoledi'";
        case kzero::Weekday::Thursday: return "giovedi'";
        case kzero::Weekday::Friday: return "venerdi'";
        case kzero::Weekday::Saturday: return "sabato";
        case kzero::Weekday::Sunday: return "domenica";
    }
    return "?";
}

std::string leggibile(const std::string& s) {
    std::string out = s;
    std::replace(out.begin(), out.end(), '_', ' ');
    return out;
}

std::string numero(double x, int decimali_fissi) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o.setf(std::ios::fixed);
    o.precision(decimali_fissi >= 0 ? decimali_fissi : 3);
    o << x;
    std::string s = o.str();
    if (decimali_fissi < 0) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

std::string testo(const Json& j) {
    if (j.is_string()) return leggibile(j.get<std::string>());
    if (j.is_boolean()) return j.get<bool>() ? "si'" : "no";
    if (j.is_number_integer()) return std::to_string(j.get<long long>());
    if (j.is_number()) return numero(j.get<double>());
    if (j.is_null()) return "-";
    return j.dump();
}

std::string euro(const Json& j) { return j.is_null() ? "non indicato" : numero(j.get<double>(), 2) + " EUR"; }

std::string quanti(long long n, const char* uno, const char* molti) {
    return n == 0 ? std::string("nessun ") + uno : std::to_string(n) + " " + (n == 1 ? uno : molti);
}

std::string durata(long long minuti) {
    const long long ore = minuti / 60;
    const long long resto = minuti % 60;
    const std::string m = std::to_string(resto) + (resto == 1 ? " minuto" : " minuti");
    if (ore == 0) return m;
    const std::string h = std::to_string(ore) + (ore == 1 ? " ora" : " ore");
    return resto == 0 ? h : h + " e " + m;
}

}  // namespace kzero::api
