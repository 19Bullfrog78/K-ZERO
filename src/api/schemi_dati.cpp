// K-Zero API - schemi dei file di dati e classi dei dati (vedi schemi_dati.hpp).
#include "schemi_dati.hpp"

#include <optional>
#include <stdexcept>
#include <vector>

#include "formato_dati.hpp"
#include "risorse.hpp"

namespace kzero::api {

bool visibile(Classe c, const Visibilita& v) {
    switch (c) {
        case Classe::Comune: return true;
        case Classe::Personale: return v.personali;
        case Classe::Particolare: return v.particolari;
    }
    return false;
}

namespace {

std::vector<std::string> parti(const std::string& puntatore) {
    std::vector<std::string> out;
    if (puntatore.empty()) return out;
    std::size_t i = 1;
    while (i <= puntatore.size()) {
        const std::size_t fine = std::min(puntatore.find('/', i), puntatore.size());
        std::string p = puntatore.substr(i, fine - i);
        for (std::size_t k = 0; k + 1 < p.size(); ++k)
            if (p[k] == '~') p.replace(k, 2, p[k + 1] == '1' ? "/" : "~");
        out.push_back(p);
        i = fine + 1;
    }
    return out;
}

std::optional<Classe> marcatura(const JsonDati& nodo) {
    const auto it = nodo.find("x-kzero-classe");
    if (it == nodo.end() || !it->is_string()) return std::nullopt;
    const std::string c = it->get<std::string>();
    if (c == "comune") return Classe::Comune;
    if (c == "personale") return Classe::Personale;
    if (c == "particolare") return Classe::Particolare;
    throw std::logic_error("classe sconosciuta nello schema: " + c);
}

// Cammina nello schema lungo il percorso; per ogni nodo attraversato chiama visita(nodo), dal piu' esterno
// al piu' interno. Per un campo con "$ref" visita prima la definizione e poi il punto in cui il campo e' usato:
// cosi' la marcatura scritta accanto al campo vale piu' di quella della definizione.
template <class Visita>
const JsonDati* cammina(const JsonDati& radice, const std::string& puntatore, Visita visita) {
    const JsonDati* nodo = &radice;
    const auto entra = [&](const JsonDati& n) {
        const JsonDati* r = n.is_object() && n.contains("$ref") ? &SchemiDati::risolvi(radice, n) : &n;
        if (r != &n) visita(*r);
        visita(n);
        return r;
    };
    nodo = entra(*nodo);
    for (const std::string& p : parti(puntatore)) {
        if (!nodo->is_object()) return nullptr;
        const auto props = nodo->find("properties");
        if (props != nodo->end() && props->contains(p)) {
            nodo = entra(props->at(p));
            continue;
        }
        const auto items = nodo->find("items");
        if (items != nodo->end() && items->is_object() && !p.empty() &&
            p.find_first_not_of("0123456789") == std::string::npos) {
            nodo = entra(*items);
            continue;
        }
        return nullptr;
    }
    return nodo;
}

}  // namespace

SchemiDati::SchemiDati() {
    for (const FileDati& f : file_dati())
        for (int v : f.versioni) {
            const std::string nome = "docs/schema/" + f.schema + ".v" + std::to_string(v) + ".schema.json";
            const auto testo = risorse::trova(nome);
            if (!testo) throw std::logic_error("schema non incorporato nel programma: " + nome);
            schemi_.emplace(std::make_pair(f.schema, v), JsonDati::parse(*testo));
        }
}

const SchemiDati& SchemiDati::incorporati() {
    static const SchemiDati s;
    return s;
}

const JsonDati* SchemiDati::schema(const std::string& nome, int versione) const {
    const auto it = schemi_.find({nome, versione});
    return it == schemi_.end() ? nullptr : &it->second;
}

const JsonDati& SchemiDati::risolvi(const JsonDati& radice, const JsonDati& nodo) {
    const JsonDati* n = &nodo;
    for (int giri = 0; n->is_object() && n->contains("$ref"); ++giri) {
        const std::string ref = n->at("$ref").get<std::string>();
        if (giri > 32 || ref.rfind("#/", 0) != 0) throw std::logic_error("riferimento non gestito nello schema: " + ref);
        n = &radice.at(JsonDati::json_pointer(ref.substr(1)));
    }
    return *n;
}

const JsonDati* SchemiDati::sottoschema(const std::string& nome, int versione, const std::string& puntatore) const {
    const JsonDati* s = schema(nome, versione);
    return s ? cammina(*s, puntatore, [](const JsonDati&) {}) : nullptr;
}

std::vector<SchemiDati::Livello> SchemiDati::livelli(const std::string& nome, int versione, const std::string& puntatore) const {
    const std::vector<std::string> pezzi = parti(puntatore);
    std::vector<Livello> out(pezzi.size() + 1);
    const JsonDati* s = schema(nome, versione);
    if (!s) return out;
    const auto livello = [s](const JsonDati& n) {
        Livello l;
        l.previsto = true;
        const JsonDati* d = n.is_object() && n.contains("$ref") ? &risolvi(*s, n) : &n;
        for (const JsonDati* x : {d, &n}) {   // la marcatura accanto al campo vale piu' di quella della definizione
            if (const auto c = marcatura(*x)) l.classe = c;
            if (x->is_object() && x->contains("x-kzero-titolare")) l.titolare = &x->at("x-kzero-titolare");
            if (x->is_object() && x->contains("x-kzero-trasportato")) l.trasportato = &x->at("x-kzero-trasportato");
        }
        return std::make_pair(l, d);
    };
    auto [radice, nodo] = livello(*s);
    out[0] = radice;
    for (std::size_t i = 0; i < pezzi.size(); ++i) {
        const std::string& p = pezzi[i];
        const JsonDati* figlio = nullptr;
        if (nodo->is_object()) {
            const auto props = nodo->find("properties");
            const auto items = nodo->find("items");
            if (props != nodo->end() && props->contains(p)) figlio = &props->at(p);
            else if (items != nodo->end() && items->is_object() && !p.empty() &&
                     p.find_first_not_of("0123456789") == std::string::npos)
                figlio = &*items;
        }
        if (!figlio) break;   // da qui in poi: non previsto dallo schema
        auto [l, d] = livello(*figlio);
        out[i + 1] = l;
        nodo = d;
    }
    return out;
}

Classe SchemiDati::classe(const std::string& nome, int versione, const std::string& puntatore) const {
    const JsonDati* s = schema(nome, versione);
    if (!s) return Classe::Particolare;
    std::optional<Classe> trovata;
    const JsonDati* fine = cammina(*s, puntatore, [&trovata](const JsonDati& n) {
        if (const auto c = marcatura(n)) trovata = c;
    });
    return fine && trovata ? *trovata : Classe::Particolare;
}

}  // namespace kzero::api
