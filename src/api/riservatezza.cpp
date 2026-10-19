// K-Zero API - riservatezza delle posizioni dei problemi (vedi riservatezza.hpp).
#include "riservatezza.hpp"

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

}  // namespace kzero::api
