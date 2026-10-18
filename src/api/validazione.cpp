// K-Zero API - controllo dei documenti con gli schemi (valijson) e messaggi in italiano (vedi validazione.hpp).
#include "validazione.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

namespace kzero::api {

namespace {

bool inizia(const std::string& s, const std::string& prefisso) { return s.rfind(prefisso, 0) == 0; }

std::string tra_apici(const std::string& s) { return "'" + s + "'"; }

// Testo tra i primi due apici: "Missing required property 'codice'." -> codice
std::string nome_tra_apici(const std::string& s) {
    const std::size_t b = s.rfind('\'');
    const std::size_t a = b == std::string::npos || b == 0 ? std::string::npos : s.rfind('\'', b - 1);
    return a == std::string::npos ? "" : s.substr(a + 1, b - a - 1);
}

// Numero dopo il cancelletto: "Failed to validate item #12 in array." -> 12
std::string numero_dopo_cancelletto(const std::string& s) {
    const std::size_t a = s.find('#');
    if (a == std::string::npos) return "";
    std::size_t b = a + 1;
    while (b < s.size() && s[b] >= '0' && s[b] <= '9') ++b;
    return s.substr(a + 1, b - a - 1);
}

std::string valore(const JsonDati& v) { return v.dump(-1, ' ', false, JsonDati::error_handler_t::replace); }

std::string elenco_valori(const JsonDati& valori) {
    std::string s;
    for (const auto& v : valori) s += (s.empty() ? "" : ", ") + valore(v);
    return s;
}

std::string nome_tipo(const std::string& t) {
    if (t == "string") return "testo";
    if (t == "integer") return "numero intero";
    if (t == "number") return "numero";
    if (t == "boolean") return "vero o falso (true o false)";
    if (t == "object") return "oggetto";
    if (t == "array") return "elenco";
    return t;
}

std::string tipo_di(const JsonDati& v) {
    switch (v.type()) {
        case JsonDati::value_t::string: return "testo";
        case JsonDati::value_t::number_integer:
        case JsonDati::value_t::number_unsigned: return "numero intero";
        case JsonDati::value_t::number_float: return "numero con decimali";
        case JsonDati::value_t::boolean: return "vero o falso";
        case JsonDati::value_t::object: return "oggetto";
        case JsonDati::value_t::array: return "elenco";
        case JsonDati::value_t::null: return "null";
        default: return "altro";
    }
}

std::string tipo_atteso(const JsonDati* s) {
    if (!s || !s->is_object()) return "un altro tipo";
    if (s->contains("const")) return valore(s->at("const"));
    if (s->contains("enum")) return "uno di: " + elenco_valori(s->at("enum"));
    const auto t = s->find("type");
    if (t == s->end()) return "un altro tipo";
    if (t->is_string()) return nome_tipo(t->get<std::string>());
    std::string out;
    for (const auto& x : *t) out += (out.empty() ? "" : " o ") + nome_tipo(x.get<std::string>());
    return out;
}

// Valore di una regola dello schema (es. "minimum"), se c'e'.
std::string regola(const JsonDati* s, const char* nome) {
    return s && s->is_object() && s->contains(nome) ? valore(s->at(nome)) : "";
}

std::string escape_puntatore(std::string s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '~') s.replace(i++, 1, "~0");
        else if (s[i] == '/') s.replace(i++, 1, "~1");
    }
    return s;
}

// Schema pronto per valijson, preparato una volta sola per schema e versione.
const valijson::Schema& schema_preparato(const std::string& nome, int versione) {
    static std::map<std::pair<std::string, int>, std::unique_ptr<valijson::Schema>> preparati;
    std::unique_ptr<valijson::Schema>& s = preparati[{nome, versione}];
    if (!s) {
        const JsonDati* radice = SchemiDati::incorporati().schema(nome, versione);
        if (!radice) throw std::logic_error("schema sconosciuto: " + nome);
        auto nuovo = std::make_unique<valijson::Schema>();
        valijson::SchemaParser lettore(valijson::SchemaParser::kDraft7);
        const valijson::adapters::NlohmannJsonAdapter adattatore(*radice);
        lettore.populateSchema(adattatore, *nuovo);
        s = std::move(nuovo);
    }
    return *s;
}

struct Grezzo {
    std::string puntatore;
    std::string messaggio;
};

// Piu' regole violate dallo stesso valore (es. un testo vuoto: troppo corto e senza la forma richiesta) danno un
// solo errore: quello che spiega meglio. I campi obbligatori mancanti invece restano tutti (sono campi diversi).
int priorita(const std::string& m) {
    if (inizia(m, "Missing required property") || inizia(m, "Missing dependency") ||
        inizia(m, "Object contains a property that could not be validated") ||
        inizia(m, "Failed to validate against schema associated with property name"))
        return -1;   // non si accorpa
    if (inizia(m, "Value type not permitted")) return 0;
    if (inizia(m, "Failed to match against any enum values") || inizia(m, "Failed to match expected value set by 'const'")) return 1;
    if (inizia(m, "String should be no") || inizia(m, "Expected number") || inizia(m, "Array should contain") ||
        inizia(m, "Object should have"))
        return 2;
    if (inizia(m, "Failed to match regex")) return 3;
    return 4;
}

// C'e' un altro errore in quel punto o piu' in basso?
bool altri_sotto(const std::vector<Grezzo>& grezzi, const std::string& puntatore, std::size_t escluso) {
    for (std::size_t i = 0; i < grezzi.size(); ++i)
        if (i != escluso && (grezzi[i].puntatore == puntatore || inizia(grezzi[i].puntatore, puntatore + "/"))) return true;
    return false;
}

}  // namespace

std::string traduci_messaggio(const std::string& m, const JsonDati* s, const JsonDati* istanza) {
    if (inizia(m, "Missing required property '") || inizia(m, "Missing dependency '"))
        return "manca il campo obbligatorio " + tra_apici(nome_tra_apici(m));
    if (inizia(m, "Object contains a property that could not be validated"))
        return "campo non previsto: " + tra_apici(nome_tra_apici(m));
    if (inizia(m, "Failed to validate against schema associated with property name '")) return "campo non ammesso in questo caso";
    if (inizia(m, "Value type not permitted by 'type' constraint"))
        return "tipo di valore sbagliato: atteso " + tipo_atteso(s) + (istanza ? ", trovato " + tipo_di(*istanza) : "");
    if (inizia(m, "Failed to match against any enum values"))
        return "valore non ammesso" + (istanza ? ": " + valore(*istanza) : std::string()) +
               (s && s->contains("enum") ? ". Valori ammessi: " + elenco_valori(s->at("enum")) : "");
    if (inizia(m, "Failed to match expected value set by 'const' constraint"))
        return "valore non ammesso" + (istanza ? ": " + valore(*istanza) : std::string()) +
               (s && s->contains("const") ? ". Valore atteso: " + valore(s->at("const")) : "");
    if (inizia(m, "Failed to match regex specified by 'pattern' constraint")) {
        std::string forma =
            s && s->contains("description") ? s->at("description").get<std::string>() : "il valore non ha la forma richiesta";
        if (!forma.empty() && forma.back() == '.') forma.pop_back();   // il punto lo mette la frase che contiene il messaggio
        return "forma non valida" + (istanza ? " (" + valore(*istanza) + ")" : std::string()) + ": " + forma;
    }
    if (inizia(m, "String should be no fewer than")) {
        const std::string n = regola(s, "minLength");
        return n == "1" ? "testo vuoto" : "testo troppo corto: almeno " + n + " caratteri";
    }
    if (inizia(m, "String should be no more than")) return "testo troppo lungo: al massimo " + regola(s, "maxLength") + " caratteri";
    if (inizia(m, "Expected number greater than or equal to")) return "valore troppo piccolo: almeno " + regola(s, "minimum");
    if (inizia(m, "Expected number greater than"))
        return "valore troppo piccolo: deve essere maggiore di " + regola(s, "exclusiveMinimum");
    if (inizia(m, "Expected number less than or equal to")) return "valore troppo grande: al massimo " + regola(s, "maximum");
    if (inizia(m, "Expected number less than"))
        return "valore troppo grande: deve essere minore di " + regola(s, "exclusiveMaximum");
    if (inizia(m, "Array should contain no fewer than"))
        return "elenco troppo corto: servono almeno " + regola(s, "minItems") + " elementi";
    if (inizia(m, "Array should contain no more than"))
        return "elenco troppo lungo: al massimo " + regola(s, "maxItems") + " elementi";
    if (inizia(m, "Elements at indexes")) return "elenco con elementi ripetuti";
    if (inizia(m, "Object should have no fewer than")) return "oggetto vuoto: scrivere almeno un campo";
    if (inizia(m, "Object should have no more than")) return "troppi campi";
    if (inizia(m, "Failed to validate item #")) return "elemento non valido";
    if (inizia(m, "Failed to validate against any schemas allowed by anyOf") ||
        inizia(m, "Failed to validate against any child schemas allowed by oneOf")) {
        // La descrizione dello schema dice quali sono le forme ammesse (es. "riposo" oppure l'elenco dei turni).
        std::string forme = s && s->is_object() && s->contains("description") ? s->at("description").get<std::string>() : "";
        if (!forme.empty() && forme.back() == '.') forme.pop_back();
        return "nessuna delle forme ammesse e' rispettata" + (forme.empty() ? std::string() : ": " + forme);
    }
    if (inizia(m, "Failed to validate against exactly one child schema"))
        return "il valore rispetta piu' forme alternative: ne e' ammessa una sola";
    return "non rispetta lo schema (" + m + ")";
}

std::vector<ErroreSchema> valida(const JsonDati& documento, const std::string& schema, int versione) {
    valijson::ValidationResults risultati;
    valijson::Validator validatore(valijson::Validator::kStrongTypes);
    const valijson::adapters::NlohmannJsonAdapter bersaglio(documento);
    validatore.validate(schema_preparato(schema, versione), bersaglio, &risultati);

    std::vector<Grezzo> grezzi;
    valijson::ValidationResults::Error e;
    while (risultati.popError(e)) grezzi.push_back({e.jsonPointer, e.description});

    // valijson ripete l'errore a ogni livello sopra il punto sbagliato ("non valido l'elemento #3",
    // "non valido il campo 'passaggi'"...): si tiene solo il punto preciso.
    std::vector<ErroreSchema> out;
    std::vector<int> priorita_out;
    for (std::size_t i = 0; i < grezzi.size(); ++i) {
        const std::string& m = grezzi[i].messaggio;
        std::string puntatore = grezzi[i].puntatore;
        if (inizia(m, "Failed to validate against a conditional schema") || inizia(m, "Failed to validate against additional properties") ||
            inizia(m, "Failed to validate against dependent schema"))
            continue;
        if (inizia(m, "Failed to validate against schema associated with property name '")) {
            // Senza errori piu' in basso e' un campo vietato in quel caso (schema "false"): l'errore e' il campo.
            const std::string figlio = puntatore + "/" + escape_puntatore(nome_tra_apici(m));
            if (altri_sotto(grezzi, figlio, i)) continue;
            puntatore = figlio;
        } else if (inizia(m, "Failed to validate item #")) {
            const std::string figlio = puntatore + "/" + numero_dopo_cancelletto(m);
            if (altri_sotto(grezzi, figlio, i)) continue;
            puntatore = figlio;
        } else if (inizia(m, "Object contains a property that could not be validated")) {
            puntatore += "/" + escape_puntatore(nome_tra_apici(m));   // il campo non previsto
        }
        const JsonDati* sotto = SchemiDati::incorporati().sottoschema(schema, versione, puntatore);
        const JsonDati::json_pointer jp(puntatore);
        const JsonDati* istanza = documento.contains(jp) ? &documento.at(jp) : nullptr;
        out.push_back({puntatore, traduci_messaggio(m, sotto, istanza), traduci_messaggio(m, sotto, nullptr)});
        priorita_out.push_back(priorita(m));
    }
    // In ordine di documento; a parita' di posizione, prima l'errore che spiega meglio.
    std::vector<std::size_t> ordine(out.size());
    for (std::size_t i = 0; i < ordine.size(); ++i) ordine[i] = i;
    std::stable_sort(ordine.begin(), ordine.end(), [&](std::size_t a, std::size_t b) {
        if (out[a].puntatore != out[b].puntatore) return prima_nel_documento(out[a].puntatore, out[b].puntatore);
        return priorita_out[a] < priorita_out[b];
    });
    std::vector<ErroreSchema> finale;
    std::set<std::string> valori_gia_segnalati;
    for (std::size_t i : ordine) {
        const ErroreSchema& e = out[i];
        const bool doppione = std::any_of(finale.begin(), finale.end(), [&](const ErroreSchema& f) {
            return f.puntatore == e.puntatore && f.messaggio == e.messaggio;
        });
        if (doppione) continue;
        if (priorita_out[i] >= 0 && !valori_gia_segnalati.insert(e.puntatore).second) continue;   // stesso valore: basta un errore
        finale.push_back(e);
    }
    return finale;
}

bool prima_nel_documento(const std::string& a, const std::string& b) {
    // Confronto parte per parte; le parti fatte di cifre (posizioni negli elenchi) come numeri.
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const std::size_t fa = std::min(a.find('/', i + 1), a.size());
        const std::size_t fb = std::min(b.find('/', j + 1), b.size());
        const std::string pa = a.substr(i, fa - i), pb = b.substr(j, fb - j);
        if (pa != pb) {
            const auto cifre = [](const std::string& p) {
                return p.size() > 1 && p.find_first_not_of("0123456789", 1) == std::string::npos;
            };
            if (cifre(pa) && cifre(pb) && pa.size() != pb.size()) return pa.size() < pb.size();
            return pa < pb;
        }
        i = fa;
        j = fb;
    }
    return a.size() - i < b.size() - j;
}

}  // namespace kzero::api
