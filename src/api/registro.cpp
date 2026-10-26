// K-Zero API - il registro degli accessi alla cassaforte (vedi registro.hpp e docs/cassaforte.md).
#include "registro.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>

#include "cassaforte_formato.hpp"
#include "tempo.hpp"

namespace kzero::api::registro {

using cripto::Bytes;
using cripto::Segreto;

namespace {

constexpr unsigned char kMagia[8] = {'K', 'Z', '-', 'R', 'E', 'G', 'I', 'S'};
constexpr std::size_t kIntestazione = sizeof kMagia + 2 + cassaforte::kId + 4;   // magia, versione, codice, chiave base
constexpr int kProfondita = 8;

[[noreturn]] void rovinato(const std::string& cosa) {
    throw ErroreRegistro("Il registro degli accessi (" + std::string(kFile) + ") " + cosa +
                         ": i dati non si aprono. Rimettere una copia buona del pacchetto (cassaforte e registro insieme).");
}

// Impronta da cui parte la catena: lega il registro alla sua cassaforte.
Bytes inizio_catena(const Bytes& id) {
    Bytes b(kMagia, kMagia + sizeof kMagia);
    b.insert(b.end(), id.begin(), id.end());
    return cripto::impronta(b);
}

bool ora_valida(const std::string& t) {
    // AAAA-MM-GGThh:mm:ssZ
    if (t.size() != 20 || t[10] != 'T' || t[13] != ':' || t[16] != ':' || t[19] != 'Z') return false;
    for (std::size_t i : {11u, 12u, 14u, 15u, 17u, 18u})
        if (t[i] < '0' || t[i] > '9') return false;
    return kzero::day_from_text(t.substr(0, 10)).has_value() && t.substr(11, 2) < "24" && t.substr(14, 2) < "60" &&
           t.substr(17, 2) < "61";
}

bool classi_valide(const std::vector<std::string>& c) {
    return std::all_of(c.begin(), c.end(), [](const std::string& x) { return x == "personali" || x == "particolari"; });
}

}  // namespace

JsonDati Voce::in_json() const {
    JsonDati j = JsonDati::object();
    j["numero"] = numero;
    j["ora"] = ora;
    j["utente"] = utente;
    j["comando"] = comando;
    j["classi_aperte"] = aperte;
    j["classi_mostrate"] = mostrate;
    j["nota"] = nota;
    j["precedente"] = precedente;
    return j;
}

bool Voce::da_json(const JsonDati& j, Voce& out) {
    if (!j.is_object() || j.size() != 8) return false;
    const auto testo = [&](const char* k) { return j.contains(k) && j.at(k).is_string(); };
    const auto elenco = [&](const char* k) {
        return j.contains(k) && j.at(k).is_array() &&
               std::all_of(j.at(k).begin(), j.at(k).end(), [](const JsonDati& x) { return x.is_string(); });
    };
    if (!j.contains("numero") || !j.at("numero").is_number_unsigned() || !testo("ora") || !testo("utente") ||
        !testo("comando") || !elenco("classi_aperte") || !elenco("classi_mostrate") || !testo("nota") || !testo("precedente"))
        return false;
    Voce v;
    const auto numero = j.at("numero").get<std::uint64_t>();
    if (numero == 0 || numero > UINT32_MAX) return false;
    v.numero = static_cast<std::uint32_t>(numero);
    v.ora = j.at("ora").get<std::string>();
    v.utente = j.at("utente").get<std::string>();
    v.comando = j.at("comando").get<std::string>();
    v.aperte = j.at("classi_aperte").get<std::vector<std::string>>();
    v.mostrate = j.at("classi_mostrate").get<std::vector<std::string>>();
    v.nota = j.at("nota").get<std::string>();
    v.precedente = j.at("precedente").get<std::string>();
    if (!ora_valida(v.ora) || !cassaforte::formato::codice_valido(v.utente) || v.comando.empty() ||
        !classi_valide(v.aperte) || !classi_valide(v.mostrate) || v.precedente.size() != 64)
        return false;
    out = std::move(v);
    return true;
}

Bytes impronta(const Voce& v) { return cripto::impronta(v.in_json().dump(-1, ' ', false, JsonDati::error_handler_t::strict)); }

Registro nuovo(const std::string& id) {
    Registro r;
    if (!cripto::da_esadecimale(id, r.id) || r.id.size() != cassaforte::kId) throw std::logic_error("registro: codice non valido");
    return r;
}

const Voce& aggiungi(Registro& r, Voce v) {
    v.numero = static_cast<std::uint32_t>(r.voci.size() + 1);
    v.precedente = cripto::esadecimale(r.voci.empty() ? inizio_catena(r.id) : impronta(r.voci.back()));
    r.voci.push_back(std::move(v));
    return r.voci.back();
}

Registro voci_dal_testo(std::string_view t, const Bytes& id, std::uint32_t ancora_voci, const Bytes& ancora_impronta) {
    Registro out;
    out.id = id;
    JsonDati elenco;
    try {
        const JsonDati::parser_callback_t limite = [](int profondita, JsonDati::parse_event_t, JsonDati&) {
            if (profondita > kProfondita) throw std::runtime_error("troppo profondo");
            return true;
        };
        elenco = JsonDati::parse(t.begin(), t.end(), limite);
    } catch (const std::exception&) {
        rovinato("ha un contenuto non valido");
    }
    if (!elenco.is_array()) rovinato("ha un contenuto non valido");
    std::string precedente = cripto::esadecimale(inizio_catena(id));
    for (const JsonDati& j : elenco) {
        Voce v;
        if (!Voce::da_json(j, v)) rovinato("ha una voce non valida");
        if (v.numero != out.voci.size() + 1) rovinato("ha le voci fuori ordine o una voce tolta");
        if (v.precedente != precedente) rovinato("ha la catena delle voci rotta (una voce tolta, cambiata o spostata)");
        precedente = cripto::esadecimale(impronta(v));
        out.voci.push_back(std::move(v));
    }
    if (ancora_voci > out.voci.size()) rovinato("ha meno voci di quelle che la cassaforte ricorda (voci tolte)");
    if (ancora_voci > 0 && impronta(out.voci[ancora_voci - 1]) != ancora_impronta)
        rovinato("non e' quello che la cassaforte ricorda (voci cambiate)");
    return out;
}

Registro leggi(std::string_view file, const cassaforte::Cassaforte& c) {
    const cassaforte::Riepilogo cf = c.riepilogo();
    Bytes id;
    cripto::da_esadecimale(cf.id, id);
    const auto* p = reinterpret_cast<const unsigned char*>(file.data());
    if (file.size() < kIntestazione + cripto::kNonce + cripto::kEtichetta || std::memcmp(p, kMagia, sizeof kMagia) != 0)
        rovinato("manca, e' vuoto o non e' un registro di K-Zero");
    cassaforte::formato::Lettore r(p, file.size());
    r.byte(sizeof kMagia);
    if (r.u16() != kVersione) rovinato("e' di una versione che questo programma non conosce");
    if (r.byte(cassaforte::kId) != id) rovinato("e' di un'altra cassaforte");
    const std::uint32_t versione_base = r.u32();
    const Bytes nonce = r.byte(cripto::kNonce);
    if (!r.ok() || versione_base == 0 || versione_base > c.versione_base()) rovinato("e' rovinato");
    Segreto chiave;
    try {
        chiave = c.chiave_registro(versione_base);
    } catch (const cassaforte::ErroreCassaforte&) {
        rovinato("e' cifrato con una chiave che la cassaforte non conosce");
    }
    const Bytes ad(p, p + kIntestazione);
    Segreto testo;
    const std::size_t inizio = kIntestazione + cripto::kNonce;
    if (!cripto::decifra(chiave, nonce.data(), p + inizio, file.size() - inizio, ad, testo))
        rovinato("e' stato cambiato da fuori di K-Zero");
    Registro out = voci_dal_testo(testo.testo(), id, cf.registro_voci, cf.registro_impronta);
    out.versione_base = versione_base;
    return out;
}

std::string scrivi(const Registro& r, const cassaforte::Cassaforte& c) { return scrivi(r, c, c.versione_base()); }

std::string scrivi(const Registro& r, const cassaforte::Cassaforte& c, std::uint32_t versione_base) {
    Bytes id;
    cripto::da_esadecimale(c.riepilogo().id, id);
    if (id != r.id) throw std::logic_error("registro: di un'altra cassaforte");
    JsonDati elenco = JsonDati::array();
    for (const Voce& v : r.voci) elenco.push_back(v.in_json());
    const std::string testo = elenco.dump(-1, ' ', false, JsonDati::error_handler_t::strict);
    cassaforte::formato::Scrittore w;
    w.byte(kMagia, sizeof kMagia);
    w.u16(kVersione);
    w.byte(id);
    w.u32(versione_base);
    const Bytes ad = w.dati();
    const Bytes nonce = cripto::casuali(cripto::kNonce);
    w.byte(nonce);
    const Segreto chiave = c.chiave_registro(versione_base);
    const Bytes cifrato = cripto::cifra(chiave, nonce.data(), reinterpret_cast<const unsigned char*>(testo.data()), testo.size(), ad);
    w.byte(cifrato);
    return std::string(w.dati().begin(), w.dati().end());
}

std::string ora_di_sistema() {
    const std::time_t t = std::time(nullptr);
    std::tm utc {};
    gmtime_r(&t, &utc);
    char testo[32];
    std::strftime(testo, sizeof testo, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return testo;
}

std::string oggi_di_sistema() { return ora_di_sistema().substr(0, 10); }

}  // namespace kzero::api::registro
