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
