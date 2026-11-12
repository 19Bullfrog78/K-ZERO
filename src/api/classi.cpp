// K-Zero API - la regola delle classi dei dati applicata ai documenti (vedi classi.hpp).
#include "classi.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include "cripto.hpp"
#include "formato_dati.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

const JsonDati& vuoto() {
    static const JsonDati v = JsonDati::object();
    return v;
}

// Un nodo dello schema visto con la sua classe: la marcatura piu' vicina (prima la definizione, poi il campo, che vince).
struct Nodo {
    const JsonDati* definizione = nullptr;   // il nodo con i "$ref" risolti
    Classe classe = Classe::Comune;
    bool marcato = false;                    // marcatura scritta proprio qui
    const JsonDati* trasportato = nullptr;   // "x-kzero-trasportato" (passeggero o casa), se c'e'
    const JsonDati* chiave = nullptr;        // "x-kzero-chiave": i campi che riconoscono un elemento senza codice
    std::string ref;                         // la definizione a cui rimanda il campo ("$ref"), es. "#/definitions/testo"
};

Classe da_testo(const std::string& c) {
    if (c == "comune") return Classe::Comune;
    if (c == "personale") return Classe::Personale;
    if (c == "particolare") return Classe::Particolare;
    throw std::logic_error("classe sconosciuta nello schema: " + c);
}

Nodo nodo_di(const JsonDati& radice, const JsonDati& nodo, Classe ereditata) {
    Nodo n;
    n.definizione = nodo.is_object() && nodo.contains("$ref") ? &SchemiDati::risolvi(radice, nodo) : &nodo;
    if (nodo.is_object() && nodo.contains("$ref") && nodo.at("$ref").is_string()) n.ref = nodo.at("$ref").get<std::string>();
    n.classe = ereditata;
    for (const JsonDati* x : {n.definizione, &nodo}) {
        if (!x->is_object()) continue;
        const auto c = x->find("x-kzero-classe");
        if (c != x->end() && c->is_string()) {
            n.classe = da_testo(c->get<std::string>());
            n.marcato = true;
        }
        const auto t = x->find("x-kzero-trasportato");
        if (t != x->end() && t->is_string()) n.trasportato = &*t;
        const auto k = x->find("x-kzero-chiave");
        if (k != x->end() && k->is_array() && !k->empty()) n.chiave = &*k;
    }
    return n;
}

// Il nodo di un campo dell'oggetto (un campo non previsto: nessuna marcatura, prende la classe del contenitore).
Nodo campo_di(const JsonDati& radice, const Nodo& n, const std::string& campo) {
    const auto props = n.definizione->is_object() ? n.definizione->find("properties") : n.definizione->end();
    if (n.definizione->is_object() && props != n.definizione->end() && props->is_object() && props->contains(campo))
        return nodo_di(radice, props->at(campo), n.classe);
    return nodo_di(radice, vuoto(), n.classe);
}

// Il nodo degli elementi di un elenco.
Nodo elemento_di(const JsonDati& radice, const Nodo& n) {
    if (n.definizione->is_object()) {
        const auto items = n.definizione->find("items");
        if (items != n.definizione->end() && items->is_object()) return nodo_di(radice, *items, n.classe);
    }
    return nodo_di(radice, vuoto(), n.classe);
}

constexpr int kProfondita = 64;   // gli schemi dei file sono molto meno profondi

void conta(const JsonDati& radice, const JsonDati& valore, const Nodo& n, ValoriPerClasse& out, int profondita) {
    if (profondita > kProfondita) return;
    if (valore.is_object()) {
        for (const auto& [k, v] : valore.items()) conta(radice, v, campo_di(radice, n, k), out, profondita + 1);
        return;
    }
    if (valore.is_array()) {
        const Nodo e = elemento_di(radice, n);
        for (const auto& v : valore) conta(radice, v, e, out, profondita + 1);
        return;
    }
    if (n.classe == Classe::Personale) ++out.personali;
    if (n.classe == Classe::Particolare) ++out.particolari;
}

// Tutto il sotto-albero ha la classe c (nessun campo marcato diversamente)?
bool uniforme(const JsonDati& radice, const JsonDati& valore, const Nodo& n, Classe c, int profondita) {
    if (n.classe != c || profondita > kProfondita) return false;
    if (valore.is_object()) {
        for (const auto& [k, v] : valore.items())
            if (!uniforme(radice, v, campo_di(radice, n, k), c, profondita + 1)) return false;
    } else if (valore.is_array()) {
        const Nodo e = elemento_di(radice, n);
        for (const auto& v : valore)
            if (!uniforme(radice, v, e, c, profondita + 1)) return false;
    }
    return true;
}

// Un dato personale di chi e' legato a un luogo sensibile vale come particolare, e sta nella sezione dei legati.
Classe effettiva(Classe c, bool promosso) { return c == Classe::Personale && promosso ? Classe::Particolare : c; }
bool legato(Classe c, bool promosso) { return c == Classe::Personale && promosso; }

bool con_codice(const Nodo& e) {
    if (!e.definizione->is_object()) return false;
    const auto req = e.definizione->find("required");
    if (req == e.definizione->end() || !req->is_array()) return false;
    return std::any_of(req->begin(), req->end(), [](const JsonDati& x) { return x.is_string() && x.get<std::string>() == "codice"; });
}

std::string impronta_di(const JsonDati& elemento) {
    return cripto::esadecimale(cripto::impronta(elemento.dump(-1, ' ', false, JsonDati::error_handler_t::strict)));
}

// La chiave di un elemento senza codice: i valori dei campi dichiarati nello schema ("x-kzero-chiave", es. turno e data
// di un'assegnazione del giorno), se ci sono tutti e sono testi o numeri; altrimenti null.
JsonDati chiave_di(const JsonDati& x, const JsonDati& campi) {
    if (!x.is_object()) return JsonDati();
    JsonDati k = JsonDati::object();
    for (const JsonDati& c : campi) {
        if (!c.is_string()) return JsonDati();
        const auto v = x.find(c.get<std::string>());
        if (v == x.end() || !(v->is_string() || v->is_number_integer())) return JsonDati();
        k[c.get<std::string>()] = *v;
    }
    return k;
}

// I passi che riconoscono gli elementi di un elenco (nella forma dei file normali), in quest'ordine:
// - il codice, quando lo schema degli elementi lo rende obbligatorio;
// - la chiave dichiarata nello schema ("x-kzero-chiave");
// - solo se lo schema non dice niente: l'impronta del contenuto comune e l'ordine tra gli elementi uguali (due elementi
//   con gli stessi dati comuni non si distinguono, e scambiarli non cambia nulla di quello che si vede).
// Un codice o una chiave ripetuti non prendono un numero d'ordine: l'unione li trova ambigui (K-CTR-094), mai un
// abbinamento indovinato.
std::vector<JsonDati> passi_di(const JsonDati& elenco, const Nodo& e) {
    std::vector<JsonDati> out;
    std::map<std::string, int> visti;
    const bool codici = con_codice(e);
    for (const JsonDati& x : elenco) {
        if (codici && x.is_object() && x.contains("codice") && x.at("codice").is_string()) {
            out.push_back(JsonDati{{"codice", x.at("codice")}});
            continue;
        }
        if (e.chiave) {
            JsonDati k = chiave_di(x, *e.chiave);
            if (!k.is_null()) {
                out.push_back(JsonDati{{"chiave", std::move(k)}});
                continue;
            }
        }
        const std::string h = impronta_di(x);
        out.push_back(JsonDati{{"contenuto", h}, {"n", visti[h]++}});
    }
    return out;
}

struct Contesto {
    const std::string& file;
    const JsonDati& radice;
    const LegameSensibile* legame;
};

void separa_in(const Contesto& ctx, JsonDati& valore, const Nodo& n, bool promosso, std::vector<Voce>& voci, int profondita) {
    if (profondita > kProfondita) throw std::logic_error("documento troppo profondo per la separazione");
    if (valore.is_object()) {
        std::vector<std::string> chiavi;
        for (const auto& [k, v] : valore.items()) chiavi.push_back(k);
        for (const std::string& k : chiavi) {
            JsonDati& v = valore.at(k);
            const Nodo f = campo_di(ctx.radice, n, k);
            if (f.classe != Classe::Comune && uniforme(ctx.radice, v, f, f.classe, profondita + 1)) {
                Voce x;
                x.file = ctx.file;
                x.campo = k;
                x.valore = std::move(v);
                x.classe = effettiva(f.classe, promosso);
                x.legato = legato(f.classe, promosso);
                voci.push_back(std::move(x));
                valore.erase(k);
                continue;
            }
            if (!v.is_object() && !v.is_array()) continue;
            const std::size_t prima = voci.size();
            const bool vuoto_prima = v.empty();
            // K-011 aggiunta 2: un campo personale dentro un dato particolare (es. motivo e chi di una dichiarazione) vale come
            // particolare e sta tra i legati: un permesso solo non lo da' mai insieme al dato particolare che lo contiene.
            separa_in(ctx, v, f, promosso || f.classe == Classe::Particolare, voci, profondita + 1);
            for (std::size_t i = prima; i < voci.size(); ++i) voci[i].percorso.insert(voci[i].percorso.begin(), k);
            // Un oggetto rimasto vuoto perche' i suoi campi sono andati nella cassaforte non resta nel file normale.
            if (v.is_object() && v.empty() && !vuoto_prima) valore.erase(k);
        }
        return;
    }
    if (!valore.is_array()) return;
    const Nodo e = elemento_di(ctx.radice, n);
    if (e.marcato && e.classe != Classe::Comune) {
        // Elementi marcati tutti interi (es. le assenze): vanno interi nella cassaforte.
        for (JsonDati& x : valore) {
            Voce v;
            v.file = ctx.file;
            v.elemento = true;
            v.valore = std::move(x);
            v.classe = effettiva(e.classe, promosso);
            v.legato = legato(e.classe, promosso);
            voci.push_back(std::move(v));
        }
        valore = JsonDati::array();
        return;
    }
    std::vector<std::vector<Voce>> per_elemento(valore.size());
    bool qualcuna = false;
    for (std::size_t i = 0; i < valore.size(); ++i) {
        JsonDati& x = valore[i];
        if (!x.is_object() && !x.is_array()) continue;
        bool p = promosso || e.classe == Classe::Particolare;   // come sopra: dentro un elemento particolare
        if (e.trasportato && ctx.legame && x.is_object() && x.contains("codice") && x.at("codice").is_string())
            p = p || ctx.legame->legato(e.trasportato->get<std::string>(), x.at("codice").get<std::string>());
        separa_in(ctx, x, e, p, per_elemento[i], profondita + 1);
        qualcuna = qualcuna || !per_elemento[i].empty();
    }
    if (!qualcuna) return;
    const std::vector<JsonDati> passi = passi_di(valore, e);
    for (std::size_t i = 0; i < valore.size(); ++i)
        for (Voce& v : per_elemento[i]) {
            v.percorso.insert(v.percorso.begin(), passi[i]);
            voci.push_back(std::move(v));
        }
}

// Come si nomina un elemento in un messaggio: il codice, la chiave ("turno T-1, data 2026-10-22") o "una riga senza
// codice". Sono tutti dati comuni.
std::string descrivi_passo(const JsonDati& passo) {
    if (passo.contains("codice")) return passo.at("codice").get<std::string>();
    if (passo.contains("chiave")) {
        std::string t;
        for (const auto& [k, v] : passo.at("chiave").items())
            t += (t.empty() ? "" : ", ") + k + " " + (v.is_string() ? v.get<std::string>() : v.dump());
        return t;
    }
    return "una riga senza codice";
}

std::string escape(const std::string& chiave) {
    std::string s;
    for (char c : chiave) s += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
    return s;
}

kzero::Problem problema(kzero::Check c, int variante, const std::string& file, const std::string& puntatore,
                        std::vector<kzero::Param> param) {
    return kzero::Problem{c, variante, kzero::Source{file, puntatore, 0, 0}, std::move(param)};
}

// --- Testi comuni (K-018) ------------------------------------------------------------------------------------------------
// Un testo libero e' un testo scritto da chi prepara i dati: un campo che rimanda alla definizione "testo" (una riga) o
// "nota" (un testo lungo) del suo schema, oppure il codice di un elemento (il campo "codice", definizione "codice",
// dell'elemento di un elenco che lo rende obbligatorio: dove l'elemento e' definito, non nei riferimenti). Valori a scelta
// fissa, date, orari, numeri e codici di riferimento hanno altre definizioni e non lo sono. E' comune se la sua classe (la
// marcatura piu' vicina, come sopra) e' comune.
bool testo_libero(const Nodo& f, const std::string& campo, bool in_elemento_con_codice) {
    if (f.ref == "#/definitions/testo" || f.ref == "#/definitions/nota") return true;
    return f.ref == "#/definitions/codice" && campo == "codice" && in_elemento_con_codice;
}

struct PosizioneTesto {
    std::string puntatore;
    std::string campo;      // "servizi[].nome"
    std::string elemento;   // codice dell'elemento piu' vicino con un codice
    std::string campo_elemento;   // il suo elenco, es. "servizi[].domanda.gruppi[]"
    std::string elenco;     // l'elenco principale del file e l'indice dell'elemento
    long long indice = -1;
    std::string radice;     // codice dell'elemento dell'elenco principale
};

void testi_in(const JsonDati& radice, const JsonDati& valore, const Nodo& n, const PosizioneTesto& pos, bool con_cod,
              std::vector<TestoComune>& out, int profondita) {
    if (profondita > kProfondita) return;
    if (valore.is_object()) {
        for (const auto& [k, v] : valore.items()) {
            const Nodo f = campo_di(radice, n, k);
            PosizioneTesto p = pos;
            p.puntatore += "/" + escape(k);
            p.campo += (p.campo.empty() ? "" : ".") + k;
            if (v.is_string()) {
                if (f.classe == Classe::Comune && testo_libero(f, k, con_cod))
                    out.push_back({p.puntatore, p.campo, p.elemento, p.campo_elemento, p.elenco, p.indice, p.radice, v.get<std::string>()});
                continue;
            }
            if (v.is_array()) {
                const Nodo e = elemento_di(radice, f);
                const bool codici = con_codice(e);
                for (std::size_t i = 0; i < v.size(); ++i) {
                    PosizioneTesto q = p;
                    q.puntatore += "/" + std::to_string(i);
                    q.campo += "[]";
                    if (profondita == 0) {   // un elemento di un elenco principale del file
                        q.elenco = k;
                        q.indice = static_cast<long long>(i);
                    }
                    const JsonDati& x = v[i];
                    const bool ha_codice = codici && x.is_object() && x.contains("codice") && x.at("codice").is_string();
                    if (ha_codice) {
                        q.elemento = x.at("codice").get<std::string>();
                        q.campo_elemento = q.campo;
                        if (profondita == 0) q.radice = q.elemento;
                    }
                    if (x.is_string()) {
                        if (e.classe == Classe::Comune && testo_libero(e, "", false))
                            out.push_back({q.puntatore, q.campo, q.elemento, q.campo_elemento, q.elenco, q.indice, q.radice, x.get<std::string>()});
                    } else {
                        testi_in(radice, x, e, q, ha_codice, out, profondita + 2);
                    }
                }
                continue;
            }
            if (v.is_object()) testi_in(radice, v, f, p, false, out, profondita + 1);
        }
    }
}

// I campi dei testi comuni di uno schema, seguendo properties, items e $ref (per la tabella di docs/controlli.md).
void campi_in(const JsonDati& radice, const Nodo& n, const std::string& campo, bool con_cod, std::set<std::string>& visti,
              std::vector<std::string>& out, int profondita) {
    if (profondita > kProfondita || !n.definizione->is_object()) return;
    const auto props = n.definizione->find("properties");
    if (props != n.definizione->end() && props->is_object())
        for (const auto& [k, v] : props->items()) {
            const Nodo f = nodo_di(radice, v, n.classe);
            const std::string c = campo + (campo.empty() ? "" : ".") + k;
            if (testo_libero(f, k, con_cod)) {
                if (f.classe == Classe::Comune && visti.insert(c).second) out.push_back(c);
                continue;
            }
            campi_in(radice, f, c, false, visti, out, profondita + 1);
        }
    const auto items = n.definizione->find("items");
    if (items != n.definizione->end() && items->is_object()) {
        const Nodo e = nodo_di(radice, *items, n.classe);
        const std::string c = campo + "[]";
        if (testo_libero(e, "", false)) {
            if (e.classe == Classe::Comune && visti.insert(c).second) out.push_back(c);
            return;
        }
        campi_in(radice, e, c, con_codice(e), visti, out, profondita + 1);
    }
}

}  // namespace

bool regole_pubbliche(const std::string& schema, int versione) {
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice || !radice->is_object()) return false;
    const auto m = radice->find("x-kzero-regole-pubbliche");
    return m != radice->end() && m->is_boolean() && m->get<bool>();
}

std::vector<TestoComune> testi_comuni(const JsonDati& documento, const std::string& schema, int versione) {
    std::vector<TestoComune> out;
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice) return out;
    testi_in(*radice, documento, nodo_di(*radice, *radice, Classe::Comune), PosizioneTesto{}, false, out, 0);
    return out;
}

std::vector<std::string> campi_testi_comuni(const std::string& schema, int versione) {
    std::vector<std::string> out;
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice) return out;
    std::set<std::string> visti;
    campi_in(*radice, nodo_di(*radice, *radice, Classe::Comune), "", false, visti, out, 0);
    return out;
}

JsonDati Voce::in_json() const {
    JsonDati j = JsonDati::object();
    j["file"] = file;
    j["percorso"] = percorso;
    if (elemento) {
        j["elemento"] = valore;
    } else {
        j["campo"] = campo;
        j["valore"] = valore;
    }
    return j;
}

bool Voce::da_json(const JsonDati& j, Classe c, Voce& out) {
    if (!j.is_object() || !j.contains("file") || !j.at("file").is_string() || !j.contains("percorso") || !j.at("percorso").is_array())
        return false;
    const std::string f = j.at("file").get<std::string>();
    if (!file_dati_per_nome(f)) return false;
    for (const JsonDati& p : j.at("percorso")) {
        if (p.is_string()) continue;
        if (!p.is_object()) return false;
        if (p.size() == 1 && p.contains("codice") && p.at("codice").is_string()) continue;
        if (p.size() == 1 && p.contains("chiave") && p.at("chiave").is_object() && !p.at("chiave").empty() &&
            std::all_of(p.at("chiave").begin(), p.at("chiave").end(),
                        [](const JsonDati& v) { return v.is_string() || v.is_number_integer(); }))
            continue;
        if (p.size() == 2 && p.contains("contenuto") && p.at("contenuto").is_string() && p.at("contenuto").get<std::string>().size() == 64 &&
            p.contains("n") && p.at("n").is_number_integer() && p.at("n").get<long long>() >= 0)
            continue;
        return false;
    }
    Voce v;
    v.file = f;
    v.percorso = j.at("percorso");
    v.classe = c;
    if (j.size() == 3 && j.contains("elemento")) {
        v.elemento = true;
        v.valore = j.at("elemento");
    } else if (j.size() == 4 && j.contains("campo") && j.at("campo").is_string() && j.contains("valore")) {
        v.campo = j.at("campo").get<std::string>();
        if (v.campo.empty()) return false;
        v.valore = j.at("valore");
    } else {
        return false;
    }
    out = std::move(v);
    return true;
}

ValoriPerClasse conta_valori(const JsonDati& documento, const std::string& schema, int versione) {
    ValoriPerClasse out;
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice) return out;
    conta(*radice, documento, nodo_di(*radice, *radice, Classe::Comune), out, 0);
    return out;
}

ValoriPerClasse conta_valori(const Voce& voce, const std::string& schema, int versione) {
    ValoriPerClasse out;
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice) return out;
    Nodo n = nodo_di(*radice, *radice, Classe::Comune);
    for (const JsonDati& p : voce.percorso) n = p.is_string() ? campo_di(*radice, n, p.get<std::string>()) : elemento_di(*radice, n);
    const Nodo finale = voce.elemento ? elemento_di(*radice, n) : campo_di(*radice, n, voce.campo);
    conta(*radice, voce.valore, finale, out, 0);
    return out;
}

std::vector<Voce> separa(const std::string& file, JsonDati& documento, const std::string& schema, int versione,
                         const LegameSensibile* legame) {
    const JsonDati* radice = SchemiDati::incorporati().schema(schema, versione);
    if (!radice) throw std::logic_error("separazione: schema sconosciuto " + schema);
    std::vector<Voce> voci;
    const Contesto ctx{file, *radice, legame};
    separa_in(ctx, documento, nodo_di(*radice, *radice, Classe::Comune), false, voci, 0);
    return voci;
}

bool unisci(const std::string& file, JsonDati& documento, const std::vector<const Voce*>& voci, std::vector<kzero::Problem>& tutti) {
    // Un problema per elemento e classe, non uno per valore (es. un elemento tolto con cinque valori nella cassaforte).
    std::vector<kzero::Problem> problemi;
    std::set<std::string> visti;
    const auto aggiungi_unico = [&](kzero::Problem p) {
        std::string chiave = std::to_string(static_cast<int>(p.check)) + "|" + std::to_string(p.variant) + "|" + p.where.pointer;
        for (const kzero::Param& x : p.params) chiave += "|" + x.text;
        if (visti.insert(chiave).second) problemi.push_back(std::move(p));
    };
    // Prima si trova il posto di ogni voce nel documento com'e' (gli indici degli elenchi non cambiano), poi si scrive.
    struct Posto {
        std::string puntatore;   // il contenitore (oggetto o elenco), anche da creare
        const Voce* voce;
    };
    std::vector<Posto> posti;
    std::map<std::string, std::vector<JsonDati>> passi_elenchi;   // puntatore di un elenco -> passi dei suoi elementi
    bool ok = true;
    const std::size_t problemi_prima = problemi.size();
    for (const Voce* v : voci) {
        const int base = v->classe == Classe::Particolare ? 1 : 0;
        std::string puntatore, esistente;
        const JsonDati* nodo = &documento;
        bool esiste = true, trovato = true;
        for (const JsonDati& passo : v->percorso) {
            if (passo.is_string()) {
                const std::string k = passo.get<std::string>();
                if (esiste && nodo->is_object() && nodo->contains(k)) {
                    nodo = &nodo->at(k);
                } else if (esiste && nodo->is_object()) {
                    esiste = false;   // un oggetto tolto perche' rimasto vuoto: si ricrea
                } else if (esiste) {
                    trovato = false;
                    break;
                }
                puntatore += "/" + escape(k);
                if (esiste) esistente = puntatore;
                continue;
            }
            if (!esiste || !nodo->is_array()) {
                trovato = false;
                break;
            }
            auto it = passi_elenchi.find(puntatore);
            if (it == passi_elenchi.end()) {
                // Per ogni elemento dell'elenco: il codice (se c'e') e l'impronta con l'ordine tra gli uguali.
                std::vector<JsonDati> passi;
                std::map<std::string, int> visti;
                for (const JsonDati& x : *nodo) {
                    const std::string h = impronta_di(x);
                    passi.push_back(JsonDati{{"codice", x.is_object() && x.contains("codice") ? x.at("codice") : JsonDati()},
                                             {"contenuto", h},
                                             {"n", visti[h]++}});
                }
                it = passi_elenchi.emplace(puntatore, std::move(passi)).first;
            }
            std::vector<std::size_t> uguali;
            for (std::size_t i = 0; i < it->second.size(); ++i) {
                const JsonDati& p = it->second[i];
                bool stesso = false;
                if (passo.contains("codice")) {
                    stesso = p.at("codice") == passo.at("codice");
                } else if (passo.contains("chiave")) {
                    const JsonDati& x = (*nodo)[i];
                    stesso = x.is_object();
                    for (const auto& [campo, valore] : passo.at("chiave").items()) {
                        const auto v = stesso ? x.find(campo) : x.end();
                        stesso = stesso && v != x.end() && *v == valore;
                    }
                } else {
                    stesso = p.at("contenuto") == passo.at("contenuto") && p.at("n") == passo.at("n");
                }
                if (stesso) uguali.push_back(i);
            }
            if (uguali.size() != 1) {
                aggiungi_unico(problema(kzero::Check::VaultValueOrphan, base + (uguali.empty() ? 0 : 2), file, esistente,
                                            {kzero::text_param("file", file),
                                             kzero::text_param("elemento", descrivi_passo(passo))}));
                trovato = false;
                ok = false;
                break;
            }
            nodo = &(*nodo)[uguali.front()];
            puntatore += "/" + std::to_string(uguali.front());
            esistente = puntatore;
        }
        if (!trovato) {
            if (problemi.size() == problemi_prima || problemi.back().check != kzero::Check::VaultValueOrphan ||
                problemi.back().where.pointer != esistente)
                aggiungi_unico(problema(kzero::Check::VaultValueOrphan, base, file, esistente,
                                            {kzero::text_param("file", file), kzero::text_param("elemento", "la parte del file che lo conteneva")}));
            ok = false;
            continue;
        }
        if (v->elemento) {
            if (!esiste || !nodo->is_array()) {
                aggiungi_unico(problema(kzero::Check::VaultValueOrphan, base, file, esistente,
                                            {kzero::text_param("file", file), kzero::text_param("elemento", "l'elenco che lo conteneva")}));
                ok = false;
                continue;
            }
        } else if (esiste && (!nodo->is_object() || nodo->contains(v->campo))) {
            aggiungi_unico(problema(kzero::Check::VaultValueConflict, base, file, esistente, {kzero::text_param("file", file)}));
            ok = false;
            continue;
        }
        posti.push_back({puntatore, v});
    }
    if (!ok) {
        tutti.insert(tutti.end(), problemi.begin(), problemi.end());
        return false;
    }
    JsonDati unito = documento;
    for (const Posto& p : posti) {
        JsonDati& contenitore = unito[JsonDati::json_pointer(p.puntatore)];
        if (p.voce->elemento) {
            contenitore.push_back(p.voce->valore);
            continue;
        }
        if (contenitore.is_null()) contenitore = JsonDati::object();
        if (contenitore.contains(p.voce->campo)) {   // due voci per lo stesso campo
            aggiungi_unico(problema(kzero::Check::VaultValueConflict, p.voce->classe == Classe::Particolare ? 1 : 0, file,
                                        p.puntatore, {kzero::text_param("file", file)}));
            tutti.insert(tutti.end(), problemi.begin(), problemi.end());
            return false;
        }
        contenitore[p.voce->campo] = p.voce->valore;
    }
    documento = std::move(unito);
    return true;
}

}  // namespace kzero::api
