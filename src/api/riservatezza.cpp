// K-Zero API - riservatezza delle posizioni dei problemi (vedi riservatezza.hpp).
#include "riservatezza.hpp"

#include <algorithm>
#include <map>
#include <vector>

#include "formato_dati.hpp"

namespace kzero::api {

Classe piu_riservata(Classe a, Classe b) { return static_cast<int>(a) >= static_cast<int>(b) ? a : b; }

const char* parametro_per(Classe c) {
    return c == Classe::Particolare ? "--includi-dati-particolari" : "--includi-dati-personali";
}

namespace {

// Schema e versione con cui guardare un file: la versione letta; per un file non letto (errore di forma) l'ultima
// versione che il programma conosce. Nessun valore per un file che non fa parte del formato.
std::optional<std::pair<std::string, int>> schema_di(const std::string& file, const Lettura* lettura) {
    const FileDati* f = file_dati_per_nome(file);
    if (!f) return std::nullopt;
    if (lettura) {
        const auto v = lettura->versioni.find(file);
        if (v != lettura->versioni.end()) return std::make_pair(f->schema, v->second);
    }
    return std::make_pair(f->schema, f->versioni.back());
}

// I primi "pezzi" pezzi di un percorso JSON: ("/persone/2/abilitazioni/1", 2) -> "/persone/2".
std::string prefisso(const std::string& puntatore, std::size_t pezzi) {
    std::size_t fine = 0;
    for (std::size_t i = 0; i < pezzi; ++i) {
        fine = puntatore.find('/', fine + 1);
        if (fine == std::string::npos) return puntatore;
    }
    return puntatore.substr(0, fine);
}

PosizioneMostrata mostra(const kzero::Source& s, const Lettura& l, const Visibilita& v, int profondita) {
    PosizioneMostrata out{s, std::nullopt};
    const auto sv = schema_di(s.file, &l);
    if (!sv) return out;
    const auto livelli = SchemiDati::incorporati().livelli(sv->first, sv->second, s.pointer);
    std::size_t taglio = livelli.size();
    for (std::size_t i = 0; i < livelli.size() && taglio == livelli.size(); ++i)
        if (livelli[i].classe && !visibile(*livelli[i].classe, v)) taglio = i;
    if (taglio == livelli.size()) return out;
    out.ridotta = *livelli[taglio].classe;
    out.posizione = kzero::Source{};
    // Un elemento con tutti i dati personali (es. un'assenza) lascia il posto all'elemento comune a cui appartiene.
    for (std::size_t j = taglio; j < livelli.size(); ++j) {
        if (!livelli[j].titolare) continue;
        const JsonDati& t = *livelli[j].titolare;
        const auto per_file = l.titolari.find(s.file);
        if (per_file == l.titolari.end()) return out;
        const auto codice = per_file->second.find(prefisso(s.pointer, j));
        if (codice == per_file->second.end()) return out;
        const std::string file = t.at("file").get<std::string>();
        const std::string elenco = t.at("elenco").get<std::string>();
        const auto codici = l.codici.find({file, elenco});
        if (codici == l.codici.end()) return out;
        const auto indice = codici->second.find(codice->second);
        if (indice == codici->second.end() || profondita > 4) return out;
        const PosizioneMostrata titolare =
            mostra(kzero::Source{file, "/" + elenco + "/" + std::to_string(indice->second), 0, 0}, l, v, profondita + 1);
        out.posizione = titolare.posizione;
        return out;
    }
    if (taglio > 0) out.posizione = kzero::Source{s.file, prefisso(s.pointer, taglio - 1), 0, 0};
    return out;
}

}  // namespace

Classe classe_della_posizione(const kzero::Source& s, const Lettura& l) {
    const auto sv = schema_di(s.file, &l);
    if (!sv) return Classe::Comune;
    Classe c = Classe::Comune;
    for (const auto& livello : SchemiDati::incorporati().livelli(sv->first, sv->second, s.pointer))
        if (livello.classe) c = piu_riservata(c, *livello.classe);
    return c;
}

PosizioneMostrata mostra_posizione(const kzero::Source& s, const Lettura& l, const Visibilita& v) { return mostra(s, l, v, 0); }

bool file_riservato(const std::string& file) {
    const auto sv = schema_di(file, nullptr);
    if (!sv) return false;
    const auto livelli = SchemiDati::incorporati().livelli(sv->first, sv->second, "");
    return livelli.front().classe && *livelli.front().classe != Classe::Comune;
}

namespace {

// Insiemi disgiunti per le corse legate dai trasbordi.
struct Insiemi {
    std::vector<std::size_t> padre;
    explicit Insiemi(std::size_t n) : padre(n) {
        for (std::size_t i = 0; i < n; ++i) padre[i] = i;
    }
    std::size_t radice(std::size_t x) {
        while (padre[x] != x) x = padre[x] = padre[padre[x]];
        return x;
    }
    void unisci(std::size_t a, std::size_t b) { padre[radice(a)] = radice(b); }
};

}  // namespace

LegameSensibile::LegameSensibile(const kzero::Index* indice) {
    if (!indice) return;
    const kzero::Package& p = indice->package();
    std::set<kzero::Code> sensibili, porta_a_porta;
    for (const kzero::Place& l : p.places) {
        if (l.sensitive) sensibili.insert(l.code);
        if (l.type == kzero::PlaceType::DoorToDoor) porta_a_porta.insert(l.code);
    }
    const bool tutto_il_porta_a_porta = p.manifest.door_to_door_sensitive;
    if (sensibili.empty() && !(tutto_il_porta_a_porta && !porta_a_porta.empty())) return;
    const auto sensibile = [&](const kzero::Code& c) { return sensibili.count(c) > 0; };
    const auto casa_pap = [&](const kzero::Code& c) { return porta_a_porta.count(c) > 0; };

    // Gruppi legati e loro passeggeri e case.
    for (const kzero::Service& s : p.services)
        for (const kzero::PassengerGroup& g : s.groups) {
            bool legato = false;
            if (g.school)
                if (const kzero::School* sc = indice->school(g.school->code)) legato = sensibile(sc->place.code);
            for (const auto& f : g.stops) legato = legato || sensibile(f.place.code);
            for (const kzero::Passenger& q : g.passengers) {
                bool lq = legato;
                for (const auto* a : {q.boarding ? &*q.boarding : nullptr, q.alighting ? &*q.alighting : nullptr})
                    if (a) lq = lq || sensibile(a->place.code) || (tutto_il_porta_a_porta && casa_pap(a->place.code));
                if (lq) passeggeri_.insert(q.code);
            }
            if (legato)
                for (const auto& f : g.stops)
                    if (casa_pap(f.place.code)) case_.insert(f.place.code);
        }
    if (tutto_il_porta_a_porta) case_.insert(porta_a_porta.begin(), porta_a_porta.end());

    // Corse: insieme alle corse legate dai trasbordi, quelle che passano da un luogo sensibile legano le case che toccano.
    Insiemi insiemi(p.trips.size());
    std::map<kzero::Code, std::size_t> trasbordo;
    for (std::size_t i = 0; i < p.trips.size(); ++i)
        for (const kzero::Passage& ps : p.trips[i].passages)
            for (const auto& h : ps.transfers) {
                const auto [it, nuovo] = trasbordo.emplace(h.code, i);
                if (!nuovo) insiemi.unisci(i, it->second);
            }
    std::set<std::size_t> radici_sensibili;
    for (std::size_t i = 0; i < p.trips.size(); ++i)
        for (const kzero::Passage& ps : p.trips[i].passages)
            if (sensibile(ps.place.code)) radici_sensibili.insert(insiemi.radice(i));
    for (std::size_t i = 0; i < p.trips.size(); ++i)
        if (radici_sensibili.count(insiemi.radice(i)))
            for (const kzero::Passage& ps : p.trips[i].passages)
                if (casa_pap(ps.place.code)) case_.insert(ps.place.code);

    // K-012: i committenti privati con un servizio legato (la regola in riservatezza.hpp). Un servizio e' legato se i suoi
    // gruppi, le loro fermate o i loro passeggeri toccano un luogo sensibile (o, con tutto il porta a porta sensibile, una casa
    // porta a porta), oppure se una sua corsa passa da un luogo sensibile, da sola o con i trasbordi (o da una casa porta a
    // porta, con tutto il porta a porta sensibile).
    std::set<kzero::Code> servizi_legati;
    for (const kzero::Service& s : p.services) {
        bool legato = false;
        const auto tocca = [&](const kzero::Code& luogo) {
            legato = legato || sensibile(luogo) || (tutto_il_porta_a_porta && casa_pap(luogo));
        };
        for (const kzero::PassengerGroup& g : s.groups) {
            if (g.school)
                if (const kzero::School* sc = indice->school(g.school->code)) tocca(sc->place.code);
            for (const auto& f : g.stops) tocca(f.place.code);
            for (const kzero::Passenger& q : g.passengers)
                for (const auto* a : {q.boarding ? &*q.boarding : nullptr, q.alighting ? &*q.alighting : nullptr})
                    if (a) tocca(a->place.code);
        }
        if (legato) servizi_legati.insert(s.code);
    }
    for (std::size_t i = 0; i < p.trips.size(); ++i) {
        bool legata = radici_sensibili.count(insiemi.radice(i)) > 0;
        if (tutto_il_porta_a_porta)
            for (const kzero::Passage& ps : p.trips[i].passages) legata = legata || casa_pap(ps.place.code);
        if (legata)
            for (const kzero::Ref& r : p.trips[i].services) servizi_legati.insert(r.code);
    }
    std::set<kzero::Code> privati;
    for (const kzero::Client& c : p.clients)
        if (c.is_private) privati.insert(c.code);
    if (!privati.empty())
        for (const kzero::Service& s : p.services) {
            if (!servizi_legati.count(s.code)) continue;
            const kzero::Contract* ct = indice->contract(s.contract.code);
            if (ct && privati.count(ct->client.code)) committenti_.insert(ct->client.code);
        }

    // Passeggeri e case si legano a vicenda, fino a quando non cambia piu' nulla.
    for (bool cambiato = true; cambiato;) {
        cambiato = false;
        for (const kzero::Service& s : p.services)
            for (const kzero::PassengerGroup& g : s.groups)
                for (const kzero::Passenger& q : g.passengers)
                    for (const auto* a : {q.boarding ? &*q.boarding : nullptr, q.alighting ? &*q.alighting : nullptr}) {
                        if (!a || !casa_pap(a->place.code)) continue;
                        const bool lq = passeggeri_.count(q.code) > 0, lc = case_.count(a->place.code) > 0;
                        if (lq && !lc) cambiato = case_.insert(a->place.code).second || cambiato;
                        if (lc && !lq) cambiato = passeggeri_.insert(q.code).second || cambiato;
                    }
    }
}

bool LegameSensibile::legato(const std::string& tipo, const kzero::Code& codice) const {
    if (tipo == "passeggero") return passeggero(codice);
    if (tipo == "casa") return casa(codice);
    if (tipo == "committente") return committente(codice);
    return false;
}

std::string trasportato(const kzero::Source& elemento, const Lettura& lettura) {
    const auto sv = schema_di(elemento.file, &lettura);
    if (!sv) return {};
    const auto livelli = SchemiDati::incorporati().livelli(sv->first, sv->second, elemento.pointer);
    const auto& ultimo = livelli.back();
    return ultimo.previsto && ultimo.trasportato && ultimo.trasportato->is_string() ? ultimo.trasportato->get<std::string>()
                                                                                   : std::string();
}

bool in_trasportato(const kzero::Source& posizione, const Lettura& lettura) {
    const auto sv = schema_di(posizione.file, &lettura);
    if (!sv) return false;
    for (const auto& l : SchemiDati::incorporati().livelli(sv->first, sv->second, posizione.pointer))
        if (l.previsto && l.trasportato) return true;
    return false;
}

}  // namespace kzero::api
