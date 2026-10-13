// K-Zero API - lettura di un pacchetto dati (vedi lettore.hpp e docs/formato-dati.md).
#include "lettore.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include "formato_dati.hpp"
#include "risorse.hpp"
#include "schemi_dati.hpp"
#include "tempo.hpp"
#include "validazione.hpp"

namespace kzero::api {

using namespace kzero;

// --- Sorgenti -------------------------------------------------------------------------------------

std::vector<Sorgente::Voce> CartellaSuDisco::voci() const {
    std::vector<Voce> v;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(cartella_, ec); !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        std::error_code ec_tipo;
        v.push_back({it->path().filename().string(), it->is_directory(ec_tipo)});
    }
    std::sort(v.begin(), v.end(), [](const Voce& a, const Voce& b) { return a.nome < b.nome; });
    return v;
}

std::optional<std::string> CartellaSuDisco::leggi(const std::string& nome) const {
    std::ifstream f(cartella_ / nome, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream testo;
    testo << f.rdbuf();
    if (f.bad()) return std::nullopt;
    return testo.str();
}

std::vector<Sorgente::Voce> PacchettoIncorporato::voci() const {
    std::vector<Voce> v;
    const std::string prefisso = cartella_ + "/";
    for (const auto& r : risorse::elenco()) {
        const std::string nome = r.nome;
        if (nome.rfind(prefisso, 0) == 0 && nome.find('/', prefisso.size()) == std::string::npos)
            v.push_back({nome.substr(prefisso.size()), false});
    }
    return v;
}

std::optional<std::string> PacchettoIncorporato::leggi(const std::string& nome) const {
    const auto testo = risorse::trova(cartella_ + "/" + nome);
    if (!testo) return std::nullopt;
    return std::string(*testo);
}

bool Lettura::forma_valida() const {
    return std::all_of(problemi.begin(), problemi.end(), [](const Problem& p) { return p.check == Check::UnknownFile; });
}

namespace {

// --- JSON senza chiavi ripetute ----------------------------------------------------------------------

std::string escape(const std::string& chiave) {
    std::string s;
    for (char c : chiave) s += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
    return s;
}

struct Analisi {
    std::optional<JsonDati> documento;
    std::vector<std::string> chiavi_ripetute;   // percorsi JSON delle chiavi ripetute
    int riga = 0, colonna = 0;                  // posizione dell'errore di sintassi
};

// nlohmann/json tiene in silenzio l'ultimo valore di una chiave ripetuta: qui la si trova e la si segnala.
Analisi analizza(const std::string& testo) {
    struct Livello {
        bool oggetto = false;
        std::set<std::string> chiavi;
        std::string chiave;
        long indice = -1;
    };
    std::vector<Livello> pila;
    Analisi a;
    const auto percorso = [&pila]() {
        std::string p;
        for (const Livello& l : pila) p += "/" + (l.oggetto ? escape(l.chiave) : std::to_string(l.indice));
        return p;
    };
    const auto nuovo_valore = [&pila]() {
        if (!pila.empty() && !pila.back().oggetto) ++pila.back().indice;
    };
    const JsonDati::parser_callback_t richiamo = [&](int, JsonDati::parse_event_t evento, JsonDati& valore) {
        switch (evento) {
            case JsonDati::parse_event_t::object_start:
            case JsonDati::parse_event_t::array_start:
                nuovo_valore();
                pila.push_back({evento == JsonDati::parse_event_t::object_start, {}, {}, -1});
                break;
            case JsonDati::parse_event_t::object_end:
            case JsonDati::parse_event_t::array_end:
                if (!pila.empty()) pila.pop_back();
                break;
            case JsonDati::parse_event_t::key: {
                const std::string chiave = valore.get<std::string>();
                pila.back().chiave = chiave;
                if (!pila.back().chiavi.insert(chiave).second) a.chiavi_ripetute.push_back(percorso());
                break;
            }
            case JsonDati::parse_event_t::value:
                nuovo_valore();
                break;
        }
        return true;
    };
    try {
        a.documento = JsonDati::parse(testo, richiamo);
    } catch (const JsonDati::parse_error& e) {
        a.documento.reset();
        a.riga = 1;
        a.colonna = 1;
        for (std::size_t i = 0; i + 1 < e.byte && i < testo.size(); ++i) {
            if (testo[i] == '\n') {
                ++a.riga;
                a.colonna = 1;
            } else if ((static_cast<unsigned char>(testo[i]) & 0xC0) != 0x80) {
                ++a.colonna;
            }
        }
    }
    return a;
}

// --- Dal JSON (gia' conforme allo schema) al modello --------------------------------------------------

class Conversione {
public:
    explicit Conversione(std::string file) : file_(std::move(file)) {}

    Source src(const std::string& p) const { return Source{file_, p, 0, 0}; }
    Ref ref(const JsonDati& o, const char* k, const std::string& p) const {
        return Ref{o.at(k).get<std::string>(), src(p + "/" + k)};
    }
    std::optional<Ref> ref_opz(const JsonDati& o, const char* k, const std::string& p) const {
        if (!o.contains(k)) return std::nullopt;
        return ref(o, k, p);
    }

    static std::string testo(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        return it == o.end() ? std::string() : it->get<std::string>();
    }
    static std::optional<int> intero(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return it->get<int>();
    }
    static std::optional<double> decimale(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return it->get<double>();
    }
    static bool vero(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        return it != o.end() && it->get<bool>();
    }
    static Day data(const JsonDati& v) {
        const auto d = day_from_text(v.get<std::string>());
        if (!d) throw std::logic_error("data non valida dopo il controllo dello schema: " + v.get<std::string>());
        return *d;
    }
    static std::optional<Day> data_opz(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return data(*it);
    }
    static Minutes orario(const JsonDati& v) {
        const auto m = minutes_from_text(v.get<std::string>());
        if (!m) throw std::logic_error("orario non valido dopo il controllo dello schema: " + v.get<std::string>());
        return *m;
    }
    static std::optional<Minutes> orario_opz(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return orario(*it);
    }
    static const JsonDati& elenco(const JsonDati& o, const char* k) {
        static const JsonDati vuoto = JsonDati::array();
        const auto it = o.find(k);
        return it == o.end() ? vuoto : *it;
    }
    static Coord coordinate(const JsonDati& c) { return Coord{c.at("lat").get<double>(), c.at("lon").get<double>()}; }
    static std::optional<Coord> coordinate_opz(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return coordinate(*it);
    }
    static std::vector<ExternalCode> esterni(const JsonDati& o) {
        std::vector<ExternalCode> v;
        for (const auto& e : elenco(o, "codici_esterni"))
            v.push_back({e.at("sistema").get<std::string>(), e.at("tipo").get<std::string>(), e.at("codice").get<std::string>()});
        return v;
    }
    static std::vector<std::string> testi(const JsonDati& o, const char* k) {
        std::vector<std::string> v;
        for (const auto& t : elenco(o, k)) v.push_back(t.get<std::string>());
        return v;
    }
    static Weekdays giorni(const JsonDati& elenco_giorni) {
        static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
        Weekdays w;
        for (const auto& g : elenco_giorni)
            for (int i = 0; i < 7; ++i)
                if (g.get<std::string>() == nomi[i]) w.add(static_cast<Weekday>(i));
        return w;
    }

private:
    std::string file_;
};

std::string indice(const std::string& base, std::size_t i) { return base + "/" + std::to_string(i); }

// Valori degli elenchi fissi del formato.
template <class E>
E valore_di(const std::vector<std::pair<const char*, E>>& tabella, const std::string& testo) {
    for (const auto& [nome, e] : tabella)
        if (testo == nome) return e;
    throw std::logic_error("valore non previsto dallo schema: " + testo);
}

const std::vector<std::pair<const char*, Grade>> kGradi = {
    {"infanzia", Grade::Kindergarten}, {"primaria", Grade::Primary},
    {"secondaria_primo_grado", Grade::LowerSecondary}, {"secondaria_secondo_grado", Grade::UpperSecondary}};
const std::vector<std::pair<const char*, GroupType>> kTipiGruppo = {
    {"studenti", GroupType::Students}, {"disabili", GroupType::Disabled}, {"utenti_linea", GroupType::LineUsers},
    {"adulti_autorizzati", GroupType::AuthorizedAdults}, {"altro", GroupType::Other}};
const std::vector<std::pair<const char*, Need>> kEsigenze = {
    {"carrozzina", Need::Wheelchair}, {"assistente", Need::Assistant}, {"altro", Need::Other}};
const std::vector<std::pair<const char*, ServiceKind>> kTipiServizio = {
    {"linea", ServiceKind::Line}, {"scolastico", ServiceKind::School}, {"disabili", ServiceKind::Disability},
    {"altro", ServiceKind::Other}};
const std::vector<std::pair<const char*, CenterType>> kTipiCentro = {
    {"diurno", CenterType::Day}, {"terapie", CenterType::Therapy}, {"socio_educativo", CenterType::SocioEducational},
    {"altro", CenterType::Other}};
const std::vector<std::pair<const char*, VehicleCategory>> kCategorie = {
    {"M1", VehicleCategory::M1}, {"M2", VehicleCategory::M2}, {"M3", VehicleCategory::M3}};

template <class E>
std::vector<E> valori_di(const std::vector<std::pair<const char*, E>>& tabella, const JsonDati& o, const char* k) {
    std::vector<E> v;
    for (const auto& t : Conversione::elenco(o, k)) v.push_back(valore_di(tabella, t.get<std::string>()));
    return v;
}

// --- File per file -----------------------------------------------------------------------------------

void manifesto(const JsonDati& d, Package& pk) {
    const Conversione c("pacchetto.json");
    Manifest& m = pk.manifest;
    m.code = d.at("codice").get<std::string>();
    m.name = c.testo(d, "nome");
    m.invented = c.vero(d, "dati_inventati");
    const auto& aziende = d.at("aziende");
    for (std::size_t i = 0; i < aziende.size(); ++i)
        m.companies.push_back({aziende[i].at("codice").get<std::string>(), aziende[i].at("nome").get<std::string>(),
                               c.src(indice("/aziende", i))});
    m.period = {c.data(d.at("periodo").at("dal")), c.data(d.at("periodo").at("al"))};
    const auto& origine = d.at("origine");
    m.origin = valore_di<Origin>({{"rilevato_a_mano", Origin::Manual}, {"calcolato", Origin::Computed},
                                  {"importato", Origin::Imported}, {"scenario", Origin::Scenario}},
                                 origine.at("tipo").get<std::string>());
    m.derived_from = c.testo(origine, "deriva_da");
    m.origin_note = c.testo(origine, "nota");
    m.level = d.at("livello").get<std::string>() == "completo" ? Level::Full : Level::NumbersOnly;
    m.timezone = d.at("fuso_orario").get<std::string>();
    m.currency = d.at("valuta").get<std::string>();
    m.transfer_margin_min = d.at("impostazioni").at("margine_trasbordo_minuti").get<int>();
    m.src = c.src("");
}

AccessLimits limiti(const JsonDati& l, const std::string& p, const Conversione& c) {
    AccessLimits a;
    a.max_length_m = c.decimale(l, "lunghezza_massima_m");
    a.max_width_m = c.decimale(l, "larghezza_massima_m");
    a.max_height_m = c.decimale(l, "altezza_massima_m");
    a.max_mass_t = c.decimale(l, "massa_massima_t");
    const auto& divieti = c.elenco(l, "divieti");
    for (std::size_t k = 0; k < divieti.size(); ++k) {
        const std::string pd = indice(p + "/divieti", k);
        AccessBan b;
        b.vehicle = c.ref_opz(divieti[k], "mezzo", pd);
        if (divieti[k].contains("categoria")) b.category = valore_di(kCategorie, divieti[k].at("categoria").get<std::string>());
        b.reason = valore_di<BanReason>({{"cartello", BanReason::Sign}, {"esperienza", BanReason::Experience}, {"altro", BanReason::Other}},
                                        divieti[k].at("motivo").get<std::string>());
        b.note = c.testo(divieti[k], "nota");
        b.src = c.src(pd);
        a.bans.push_back(b);
    }
    return a;
}

void luoghi(const JsonDati& d, Package& pk) {
    const Conversione c("luoghi.json");
    const auto& pubblici = c.elenco(d, "luoghi");
    for (std::size_t i = 0; i < pubblici.size(); ++i) {
        const auto& l = pubblici[i];
        const std::string p = indice("/luoghi", i);
        Place x;
        x.code = l.at("codice").get<std::string>();
        x.type = valore_di<PlaceType>({{"fermata", PlaceType::Stop}, {"scuola", PlaceType::School}, {"centro", PlaceType::Center},
                                      {"rimessa", PlaceType::Depot}, {"ricovero_esterno", PlaceType::ExternalShelter},
                                      {"punto_sosta", PlaceType::Parking}, {"punto_trasbordo", PlaceType::TransferPoint}},
                                     l.at("tipo").get<std::string>());
        x.name = l.at("nome").get<std::string>();
        x.alt_names = c.testi(l, "nomi_alternativi");
        x.municipality = l.at("comune").get<std::string>();
        x.coord = c.coordinate_opz(l, "coordinate");
        x.external = c.esterni(l);
        if (l.contains("limiti_accesso")) x.limits = limiti(l.at("limiti_accesso"), p + "/limiti_accesso", c);
        if (l.contains("trasbordo")) {
            const auto& t = l.at("trasbordo");
            x.transfer = TransferAuthorization{t.at("autorizzato_da").get<std::string>(), c.data(t.at("data")),
                                               c.testo(t, "documento"), c.vero(t, "a_voce"), c.testo(t, "nota"),
                                               c.src(p + "/trasbordo")};
        }
        x.note = c.testo(l, "nota");
        x.src = c.src(p);
        pk.places.push_back(x);
    }
    const auto& privati = c.elenco(d, "luoghi_privati");
    for (std::size_t i = 0; i < privati.size(); ++i) {
        const auto& l = privati[i];
        Place x;
        x.code = l.at("codice").get<std::string>();
        x.is_private = true;
        x.type = l.at("tipo").get<std::string>() == "porta_a_porta" ? PlaceType::DoorToDoor : PlaceType::ExternalShelter;
        x.name = c.testo(l, "nome");
        x.address = c.testo(l, "indirizzo");
        x.municipality = c.testo(l, "comune");
        x.coord = c.coordinate_opz(l, "coordinate");
        x.external = c.esterni(l);
        x.note = c.testo(l, "nota");
        x.src = c.src(indice("/luoghi_privati", i));
        pk.places.push_back(x);
    }
}

void tratti(const JsonDati& d, Package& pk) {
    const Conversione c("tratti.json");
    const auto& v = d.at("tratti");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/tratti", i);
        RoadSegment s;
        s.code = v[i].at("codice").get<std::string>();
        s.name = c.testo(v[i], "nome");
        s.from = c.ref(v[i], "da", p);
        s.to = c.ref(v[i], "a", p);
        s.both_directions = v[i].at("entrambi_i_sensi").get<bool>();
        s.limits = limiti(v[i].at("limiti"), p + "/limiti", c);
        s.note = c.testo(v[i], "nota");
        s.src = c.src(p);
        pk.segments.push_back(s);
    }
}

RuleSource fonte(const JsonDati& f) {
    return RuleSource{Conversione::testo(f, "documento"), f.at("punto").get<std::string>(), Conversione::testo(f, "riferimenti")};
}

PassengerCondition condizione(const JsonDati& j) {
    PassengerCondition c;
    c.grades = valori_di(kGradi, j, "gradi");
    c.group_types = valori_di(kTipiGruppo, j, "tipi_gruppo");
    c.needs = valori_di(kEsigenze, j, "esigenze");
    c.service_kinds = valori_di(kTipiServizio, j, "tipi_servizio");
    c.center_types = valori_di(kTipiCentro, j, "tipi_centro");
    c.seats_over = Conversione::intero(j, "posti_passeggeri_oltre");
    return c;
}

PassengerSet insieme(const JsonDati& j) {
    return PassengerSet{valori_di(kGradi, j, "gradi"), valori_di(kTipiGruppo, j, "tipi_gruppo")};
}

void regolamenti(const JsonDati& d, Package& pk) {
    const Conversione c("regolamenti.json");
    const auto& v = d.at("regolamenti");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& r = v[i];
        const std::string p = indice("/regolamenti", i);
        const std::string pp = p + "/parametri";
        const auto& par = r.at("parametri");
        Regulation x;
        x.code = r.at("codice").get<std::string>();
        x.authority = r.at("ente").get<std::string>();
        x.title = r.at("titolo").get<std::string>();
        x.document = r.at("documento").get<std::string>();
        x.valid_from = c.data_opz(r, "valido_dal");
        x.valid_to = c.data_opz(r, "valido_al");
        const auto valore = [&](const char* k) -> std::optional<ValueRule> {
            if (!par.contains(k)) return std::nullopt;
            return ValueRule{par.at(k).at("valore").get<int>(), fonte(par.at(k).at("fonte")), c.src(pp + "/" + k)};
        };
        const auto per_grado = [&](const char* k) -> std::optional<GradeValuesRule> {
            if (!par.contains(k)) return std::nullopt;
            GradeValuesRule g;
            for (const auto& gv : par.at(k).at("valori")) g.values.push_back({valori_di(kGradi, gv, "gradi"), gv.at("valore").get<int>()});
            g.source = fonte(par.at(k).at("fonte"));
            g.src = c.src(pp + "/" + k);
            return g;
        };
        const auto regola = [&](const char* k) -> std::optional<ConditionsRule> {
            if (!par.contains(k)) return std::nullopt;
            ConditionsRule cr;
            for (const auto& cond : par.at(k).at("condizioni")) cr.conditions.push_back(condizione(cond));
            cr.source = fonte(par.at(k).at("fonte"));
            cr.src = c.src(pp + "/" + k);
            return cr;
        };
        x.max_wait_before_min = valore("attesa_massima_prima_minuti");
        x.max_wait_after_min = valore("attesa_massima_dopo_minuti");
        x.max_home_stop_m = valore("distanza_massima_casa_fermata_m");
        x.max_stop_school_m = valore("distanza_massima_fermata_scuola_m");
        x.min_stop_distance_m = valore("distanza_minima_tra_fermate_m");
        x.min_home_school_m = per_grado("distanza_minima_casa_scuola_m");
        x.min_pupils = per_grado("alunni_minimi_per_servizio");
        x.max_ride_min = per_grado("durata_massima_a_bordo_minuti");
        x.assistant = regola("assistente_a_bordo");
        x.adult_at_dropoff = regola("adulto_alla_discesa");
        if (par.contains("discesa_senza_adulto_con_autorizzazione")) {
            const auto& j = par.at("discesa_senza_adulto_con_autorizzazione");
            x.dropoff_without_adult = GradesRule{valori_di(kGradi, j, "gradi"), fonte(j.at("fonte")),
                                                 c.src(pp + "/discesa_senza_adulto_con_autorizzazione")};
        }
        if (par.contains("passeggeri_in_piedi")) {
            const auto& j = par.at("passeggeri_in_piedi");
            x.standing = StandingRule{valori_di(kGradi, j, "non_ammessi_per_gradi"), c.intero(j, "durata_massima_minuti"),
                                      c.decimale(j, "km_massimi"), fonte(j.at("fonte")), c.src(pp + "/passeggeri_in_piedi")};
        }
        if (par.contains("combinazioni_vietate")) {
            const auto& j = par.at("combinazioni_vietate");
            CombinationsRule cr;
            for (const auto& k : j.at("combinazioni")) cr.combinations.push_back({insieme(k.at("primo")), insieme(k.at("secondo"))});
            cr.source = fonte(j.at("fonte"));
            cr.src = c.src(pp + "/combinazioni_vietate");
            x.forbidden = cr;
        }
        x.src = c.src(p);
        pk.regulations.push_back(x);
    }
}

void calendari(const JsonDati& d, Package& pk) {
    const Conversione c("calendari.json");
    const auto& v = d.at("calendari");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& k = v[i];
        const std::string p = indice("/calendari", i);
        Calendar x;
        x.code = k.at("codice").get<std::string>();
        x.name = c.testo(k, "nome");
        x.external = c.esterni(k);
        x.base = c.ref_opz(k, "base", p);
        x.range = {c.data(k.at("dal")), c.data(k.at("al"))};
        x.days = c.giorni(k.at("giorni"));
        const auto date = [&](const char* campo, std::vector<DayEntry>& out) {
            const auto& e = c.elenco(k, campo);
            for (std::size_t j = 0; j < e.size(); ++j) out.push_back({c.data(e[j]), c.src(indice(p + "/" + campo, j))});
        };
        date("date_aggiunte", x.added);
        date("date_tolte", x.removed);
        const auto& sosp = c.elenco(k, "sospensioni");
        for (std::size_t j = 0; j < sosp.size(); ++j)
            x.suspensions.push_back({{c.data(sosp[j].at("dal")), c.data(sosp[j].at("al"))}, sosp[j].at("motivo").get<std::string>(),
                                     c.src(indice(p + "/sospensioni", j))});
        x.src = c.src(p);
        pk.calendars.push_back(x);
    }
}

void scuole(const JsonDati& d, Package& pk) {
    const Conversione c("scuole.json");
    const auto& v = d.at("scuole");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& s = v[i];
        const std::string p = indice("/scuole", i);
        School x;
        x.code = s.at("codice").get<std::string>();
        x.name = s.at("nome").get<std::string>();
        x.place = c.ref(s, "luogo", p);
        x.kind = s.at("tipo").get<std::string>() == "scuola" ? InstituteKind::School : InstituteKind::Center;
        if (s.contains("grado")) x.grade = valore_di(kGradi, s.at("grado").get<std::string>());
        if (s.contains("tipo_centro")) {
            x.center_type = valore_di(kTipiCentro, s.at("tipo_centro").at("valore").get<std::string>());
            x.center_type_description = c.testo(s.at("tipo_centro"), "descrizione");
        }
        x.external = c.esterni(s);
        const auto& orari = s.at("orari");
        for (std::size_t j = 0; j < orari.size(); ++j) {
            const auto& o = orari[j];
            const std::string po = indice(p + "/orari", j);
            Timetable t;
            t.period = {c.data(o.at("dal")), c.data(o.at("al"))};
            t.days = c.giorni(o.at("giorni"));
            t.start = c.orario(o.at("inizio"));
            t.end = c.orario(o.at("fine"));
            t.arrive_by = c.orario_opz(o, "arrivo_entro");
            if (o.contains("attesa_concordata")) {
                const auto& a = o.at("attesa_concordata");
                t.wait = AgreedWait{a.at("prima_minuti").get<int>(), a.at("dopo_minuti").get<int>(),
                                    a.at("accordo").get<std::string>(), c.src(po + "/attesa_concordata")};
            }
            for (const auto& a : c.elenco(o, "orari_autorizzati"))
                t.authorized.push_back({a.at("tipo").get<std::string>() == "arrivo" ? AuthorizedTimeKind::Arrival : AuthorizedTimeKind::Pickup,
                                        c.orario(a.at("orario")), a.at("accordo").get<std::string>()});
            t.src = c.src(po);
            x.timetables.push_back(t);
        }
        x.max_ride_min = c.intero(s, "durata_massima_a_bordo_minuti");
        x.earliest_boarding = c.orario_opz(s, "salita_non_prima_di");
        x.src = c.src(p);
        pk.schools.push_back(x);
    }
}

void committenti(const JsonDati& d, Package& pk) {
    const Conversione c("committenti.json");
    const auto& v = d.at("committenti");
    for (std::size_t i = 0; i < v.size(); ++i)
        pk.clients.push_back({v[i].at("codice").get<std::string>(), v[i].at("nome").get<std::string>(), c.esterni(v[i]),
                              c.src(indice("/committenti", i))});
}

VehicleRules regole(const JsonDati& o, const std::string& p, const Conversione& c) {
    VehicleRules r;
    r.src = c.src(p);
    if (!o.contains("regole")) return r;
    const auto& j = o.at("regole");
    r.min_seats = c.intero(j, "posti_minimi");
    r.max_seats = c.intero(j, "posti_massimi");
    if (j.contains("pedana_obbligatoria")) r.ramp_required = j.at("pedana_obbligatoria").get<bool>();
    r.max_ride_min = c.intero(j, "durata_massima_a_bordo_minuti");
    r.recognized_category = c.testo(j, "categoria_riconosciuta");
    r.src = c.src(p + "/regole");
    return r;
}

void contratti(const JsonDati& d, Package& pk) {
    const Conversione c("contratti.json");
    const auto& v = d.at("contratti");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& k = v[i];
        const std::string p = indice("/contratti", i);
        Contract x;
        x.code = k.at("codice").get<std::string>();
        x.name = c.testo(k, "nome");
        x.client = c.ref(k, "committente", p);
        x.company = c.ref(k, "azienda", p);
        x.regulation = c.ref(k, "regolamento", p);
        x.valid_from = c.data_opz(k, "valido_dal");
        x.valid_to = c.data_opz(k, "valido_al");
        x.escort_provider = k.at("assistente_fornito_da").get<std::string>() == "azienda" ? EscortProvider::Company : EscortProvider::Client;
        x.rules = regole(k, p, c);
        x.external = c.esterni(k);
        x.src = c.src(p);
        pk.contracts.push_back(x);
    }
}

void gruppi_servizi(const JsonDati& d, Package& pk) {
    const Conversione c("gruppi_servizi.json");
    const auto& v = d.at("gruppi_servizi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/gruppi_servizi", i);
        ServiceLot x;
        x.code = v[i].at("codice").get<std::string>();
        x.name = c.testo(v[i], "nome");
        x.contract = c.ref(v[i], "contratto", p);
        const auto& s = v[i].at("servizi");
        for (std::size_t k = 0; k < s.size(); ++k) x.services.push_back({s[k].get<std::string>(), c.src(indice(p + "/servizi", k))});
        x.src = c.src(p);
        pk.lots.push_back(x);
    }
}

Passenger passeggero(const JsonDati& j, const std::string& p, const Conversione& c) {
    Passenger x;
    x.code = j.at("codice").get<std::string>();
    x.name = c.testo(j, "nome");
    if (j.contains("casa")) {
        const auto& h = j.at("casa");
        x.home = Home{c.testo(h, "indirizzo"), c.testo(h, "comune"), c.coordinate_opz(h, "coordinate")};
    }
    const auto& stato = j.at("stato");
    const std::string tipo = stato.at("tipo").get<std::string>();
    x.status = tipo == "attivo" ? PassengerStatus::Active : tipo == "sospeso" ? PassengerStatus::Suspended : PassengerStatus::Unlocked;
    x.status_reference = c.testo(stato, "riferimento");
    const auto assegnazione = [&](const char* k) -> std::optional<StopAssignment> {
        if (!j.contains(k)) return std::nullopt;
        const auto& a = j.at(k);
        return StopAssignment{c.ref(a, "luogo", p + "/" + k),
                              a.at("assegnazione").get<std::string>() == "bloccata" ? Assignment::Locked : Assignment::Proposed};
    };
    x.boarding = assegnazione("salita");
    x.alighting = assegnazione("discesa");
    x.reason = c.testo(j, "motivo");
    if (j.contains("esigenze")) {
        const auto& e = j.at("esigenze");
        x.needs = Needs{c.vero(e, "carrozzina"), c.vero(e, "assistente"), c.testo(e, "altro")};
    }
    x.src = c.src(p);
    return x;
}

void servizi(const JsonDati& d, Package& pk) {
    const Conversione c("servizi.json");
    const auto& v = d.at("servizi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& s = v[i];
        const std::string p = indice("/servizi", i);
        Service x;
        x.code = s.at("codice").get<std::string>();
        x.name = c.testo(s, "nome");
        x.external = c.esterni(s);
        x.kind = valore_di(kTipiServizio, s.at("tipo").get<std::string>());
        x.kind_description = c.testo(s, "tipo_descrizione");
        x.contract = c.ref(s, "contratto", p);
        x.calendar = c.ref(s, "calendario", p);
        const auto& lib = s.at("liberta_oraria");
        x.freedom = lib.at("tipo").get<std::string>() == "libera" ? TimeFreedom::Free : TimeFreedom::Fixed;
        x.agree_threshold_min = c.intero(lib, "soglia_da_concordare_minuti");
        x.rules = regole(s, p, c);
        const auto& gruppi = s.at("domanda").at("gruppi");
        for (std::size_t k = 0; k < gruppi.size(); ++k) {
            const auto& g = gruppi[k];
            const std::string pg = indice(p + "/domanda/gruppi", k);
            PassengerGroup y;
            y.code = g.at("codice").get<std::string>();
            y.name = c.testo(g, "nome");
            y.type = valore_di(kTipiGruppo, g.at("tipo").get<std::string>());
            y.type_description = c.testo(g, "tipo_descrizione");
            y.direction = g.at("verso").get<std::string>() == "ritorno" ? Direction::Return : Direction::Outbound;
            y.school = c.ref_opz(g, "scuola", pg);
            if (g.contains("finestra"))
                y.window = TimeWindow{c.orario(g.at("finestra").at("dalle")), c.orario(g.at("finestra").at("alle"))};
            const std::string conteggi = g.at("conteggi").get<std::string>();
            y.counts = conteggi == "esatti" ? CountKind::Exact : conteggi == "stimati" ? CountKind::Estimated : CountKind::Unknown;
            const auto& fermate = g.at("fermate");
            for (std::size_t f = 0; f < fermate.size(); ++f) {
                const std::string pf = indice(pg + "/fermate", f);
                y.stops.push_back({c.ref(fermate[f], "luogo", pf), c.intero(fermate[f], "salgono").value_or(0),
                                   c.intero(fermate[f], "scendono").value_or(0), c.intero(fermate[f], "carrozzine").value_or(0),
                                   c.src(pf)});
            }
            y.assistant_required = c.vero(g, "assistente_richiesto");
            const auto& pass = c.elenco(g, "passeggeri");
            for (std::size_t q = 0; q < pass.size(); ++q) y.passengers.push_back(passeggero(pass[q], indice(pg + "/passeggeri", q), c));
            y.src = c.src(pg);
            x.groups.push_back(y);
        }
        x.src = c.src(p);
        pk.services.push_back(x);
    }
}

void corse(const JsonDati& d, Package& pk) {
    const Conversione c("corse.json");
    const auto& v = d.at("corse");
    pk.trips.reserve(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& t = v[i];
        const std::string p = indice("/corse", i);
        Trip x;
        x.code = t.at("codice").get<std::string>();
        x.name = c.testo(t, "nome");
        x.external = c.esterni(t);
        const auto& serv = t.at("servizi");
        for (std::size_t k = 0; k < serv.size(); ++k) x.services.push_back({serv[k].get<std::string>(), c.src(indice(p + "/servizi", k))});
        x.calendar = c.ref(t, "calendario", p);
        const auto& passaggi = t.at("passaggi");
        x.passages.reserve(passaggi.size());
        for (std::size_t k = 0; k < passaggi.size(); ++k) {
            const auto& j = passaggi[k];
            const std::string pp = indice(p + "/passaggi", k);
            Passage y;
            y.place = c.ref(j, "luogo", pp);
            y.time = c.orario(j.at("orario"));
            y.departure = c.orario_opz(j, "partenza");
            y.transit = c.vero(j, "transito");
            const auto& persone = c.elenco(j, "persone");
            for (std::size_t q = 0; q < persone.size(); ++q) {
                const std::string pq = indice(pp + "/persone", q);
                y.persons.push_back({c.ref(persone[q], "gruppo", pq), c.intero(persone[q], "salgono").value_or(0),
                                     c.intero(persone[q], "scendono").value_or(0), c.src(pq)});
            }
            const std::string assistente = c.testo(j, "assistente");
            y.assistant = assistente == "sale" ? AssistantMove::Boards : assistente == "scende" ? AssistantMove::Alights : AssistantMove::None;
            const auto& trasbordi = c.elenco(j, "trasbordi");
            for (std::size_t q = 0; q < trasbordi.size(); ++q) {
                const auto& h = trasbordi[q];
                const std::string ph = indice(pp + "/trasbordi", q);
                y.transfers.push_back({h.at("codice").get<std::string>(),
                                       h.at("azione").get<std::string>() == "consegna" ? TransferAction::Deliver : TransferAction::Receive,
                                       c.ref(h, "gruppo", ph), h.at("persone").get<int>(), c.ref_opz(h, "passeggero", ph), c.src(ph)});
            }
            if (j.contains("km")) {
                y.loaded_km = c.decimale(j.at("km"), "pieni");
                y.empty_km = c.decimale(j.at("km"), "vuoti");
            }
            y.src = c.src(pp);
            x.passages.push_back(std::move(y));
        }
        if (t.contains("km_totali")) {
            x.total_loaded_km = c.decimale(t.at("km_totali"), "pieni");
            x.total_empty_km = c.decimale(t.at("km_totali"), "vuoti");
        }
        const auto& coinc = c.elenco(t, "coincidenze");
        for (std::size_t k = 0; k < coinc.size(); ++k) {
            const std::string pc = indice(p + "/coincidenze", k);
            x.connections.push_back({c.ref(coinc[k], "luogo", pc), coinc[k].at("servizio_esterno").get<std::string>(),
                                     c.orario(coinc[k].at("orario")),
                                     coinc[k].at("tipo").get<std::string>() == "aspettare_arrivo" ? ConnectionKind::WaitArrival
                                                                                                  : ConnectionKind::GuaranteeDeparture,
                                     c.src(pc)});
        }
        x.display_text = c.testo(t, "testo_esposto");
        x.src = c.src(p);
        pk.trips.push_back(std::move(x));
    }
}

void mezzi(const JsonDati& d, Package& pk) {
    const Conversione c("mezzi.json");
    const auto& v = d.at("mezzi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& m = v[i];
        const std::string p = indice("/mezzi", i);
        Vehicle x;
        x.code = m.at("codice").get<std::string>();
        x.company = c.ref(m, "azienda", p);
        x.plate = m.at("targa").get<std::string>();
        x.external = c.esterni(m);
        x.category = valore_di(kCategorie, m.at("categoria").get<std::string>());
        x.seats = m.at("posti").get<int>();
        x.standing = c.intero(m, "posti_in_piedi").value_or(0);
        x.wheelchair_places = c.intero(m, "carrozzine").value_or(0);
        x.seats_lost_per_wheelchair = c.intero(m, "posti_persi_per_carrozzina").value_or(0);
        x.has_ramp = c.vero(m, "pedana");
        x.ramp_seconds = c.intero(m, "tempo_pedana_secondi").value_or(0);
        x.trailer = c.vero(m, "rimorchio");
        if (m.contains("misure")) {
            const auto& j = m.at("misure");
            x.dims = Dimensions{c.decimale(j, "lunghezza_m"), c.decimale(j, "larghezza_m"), c.decimale(j, "altezza_m"),
                                c.decimale(j, "massa_t"), c.decimale(j, "sbalzo_posteriore_m")};
        }
        if (m.contains("alimentazione")) {
            const auto& j = m.at("alimentazione");
            x.power.fuel = valore_di<Fuel>({{"gasolio", Fuel::Diesel}, {"benzina", Fuel::Petrol}, {"metano", Fuel::Methane},
                                            {"gpl", Fuel::Lpg}, {"ibrido", Fuel::Hybrid}, {"elettrico", Fuel::Electric},
                                            {"idrogeno", Fuel::Hydrogen}, {"altro", Fuel::Other}},
                                           j.at("tipo").get<std::string>());
            x.power.description = c.testo(j, "descrizione");
            x.power.battery_kwh = c.decimale(j, "batteria_kwh");
            x.power.consumption_kwh_km = c.decimale(j, "consumo_kwh_km");
            x.power.min_charge_percent = c.intero(j, "carica_minima_percento");
        }
        x.daily_fixed_cost = c.decimale(m, "costo_fisso_giornaliero");
        x.cost_per_km = c.decimale(m, "costo_al_km");
        x.hypothetical = c.vero(m, "ipotetico");
        x.src = c.src(p);
        pk.vehicles.push_back(x);
    }
}

void rimesse(const JsonDati& d, Package& pk) {
    const Conversione c("rimesse.json");
    const auto& v = d.at("rimesse");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/rimesse", i);
        pk.depots.push_back({v[i].at("codice").get<std::string>(), c.ref(v[i], "luogo", p), c.ref(v[i], "azienda", p),
                             v[i].at("ricarica").get<bool>(), c.intero(v[i], "posti_mezzi"), c.src(p)});
    }
}

void tempi_sosta(const JsonDati& d, Package& pk) {
    const Conversione c("tempi_sosta.json");
    const auto& t = d.at("tempi");
    DwellTimes x;
    x.per_stop_s = c.intero(t, "fissa_per_fermata_secondi");
    for (const auto& j : c.elenco(t, "per_passeggero")) {
        const std::string tipo = j.at("tipo").get<std::string>();
        x.per_passenger.push_back({tipo == "studente" ? DwellKind::Student : tipo == "infanzia" ? DwellKind::Kindergarten : DwellKind::Disabled,
                                   j.at("salita_secondi").get<int>(), j.at("discesa_secondi").get<int>()});
    }
    x.school_unload_s = c.intero(t, "scarico_a_scuola_secondi");
    x.src = c.src("/tempi");
    pk.dwell = x;
}

void converti(const std::string& file, const JsonDati& d, Package& pk) {
    using Funzione = void (*)(const JsonDati&, Package&);
    static const std::vector<std::pair<const char*, Funzione>> conversioni = {
        {"pacchetto.json", manifesto}, {"luoghi.json", luoghi}, {"tratti.json", tratti},
        {"regolamenti.json", regolamenti}, {"calendari.json", calendari}, {"scuole.json", scuole},
        {"committenti.json", committenti}, {"contratti.json", contratti}, {"gruppi_servizi.json", gruppi_servizi},
        {"servizi.json", servizi}, {"corse.json", corse}, {"mezzi.json", mezzi}, {"rimesse.json", rimesse},
        {"tempi_sosta.json", tempi_sosta}};
    for (const auto& [nome, f] : conversioni)
        if (file == nome) return f(d, pk);
    throw std::logic_error("file del formato senza conversione: " + file);
}

Problem problema(Check c, const std::string& file, std::string puntatore, std::vector<Param> param = {}, int variante = 0) {
    return Problem{c, variante, Source{file, std::move(puntatore), 0, 0}, std::move(param)};
}

}  // namespace

Lettura leggi_pacchetto(const Sorgente& sorgente) {
    Lettura out;
    std::set<std::string>& presenti = out.presenti;
    for (const auto& v : sorgente.voci()) {
        if (!v.cartella && file_dati_per_nome(v.nome)) {
            presenti.insert(v.nome);
            continue;
        }
        out.problemi.push_back(problema(Check::UnknownFile, v.nome, "", {text_param("file", v.nome)}, v.cartella ? 1 : 0));
    }
    if (!presenti.count("pacchetto.json")) out.problemi.push_back(problema(Check::MissingManifest, "pacchetto.json", ""));

    for (const FileDati& f : file_dati()) {
        if (!presenti.count(f.file)) continue;
        const std::optional<std::string> testo = sorgente.leggi(f.file);
        if (!testo) {
            out.problemi.push_back(problema(Check::UnreadableFile, f.file, ""));
            continue;
        }
        Analisi a = analizza(*testo);
        if (!a.documento) {
            Problem p = problema(Check::InvalidJson, f.file, "", {number_param("riga", a.riga), number_param("colonna", a.colonna)});
            p.where.line = a.riga;
            p.where.column = a.colonna;
            out.problemi.push_back(p);
            continue;
        }
        if (!a.chiavi_ripetute.empty()) {
            for (const std::string& p : a.chiavi_ripetute) out.problemi.push_back(problema(Check::DuplicateKey, f.file, p));
            continue;
        }
        const JsonDati& d = *a.documento;
        if (!d.is_object() || !d.contains("schema") || !d.at("schema").is_string()) {
            out.problemi.push_back(problema(Check::WrongSchema, f.file, d.is_object() ? "/schema" : "",
                                            {text_param("atteso", f.schema)}, 0));
            continue;
        }
        if (d.at("schema").get<std::string>() != f.schema) {
            out.problemi.push_back(problema(Check::WrongSchema, f.file, "/schema",
                                            {text_param("trovato", d.at("schema").get<std::string>()), text_param("atteso", f.schema)}, 1));
            continue;
        }
        std::string supportate;
        for (int v : f.versioni) supportate += (supportate.empty() ? "" : ", ") + std::to_string(v);
        const auto versione = d.find("schema_versione");
        if (versione == d.end() || !versione->is_number_integer()) {
            out.problemi.push_back(problema(Check::UnsupportedVersion, f.file, versione == d.end() ? "" : "/schema_versione",
                                            {text_param("supportate", supportate)}, 1));
            continue;
        }
        const long long v = versione->get<long long>();
        if (std::find(f.versioni.begin(), f.versioni.end(), v) == f.versioni.end()) {
            out.problemi.push_back(problema(Check::UnsupportedVersion, f.file, "/schema_versione",
                                            {number_param("versione", v), text_param("supportate", supportate)}, 0));
            continue;
        }
        const std::vector<ErroreSchema> errori = valida(d, f.schema, static_cast<int>(v));
        if (!errori.empty()) {
            for (const ErroreSchema& e : errori) {
                // Il valore sbagliato di un campo personale o particolare segue le classi dei dati.
                Param dettaglio = text_param("dettaglio", e.messaggio);
                const Classe classe = SchemiDati::incorporati().classe(f.schema, static_cast<int>(v), e.puntatore);
                if (classe != Classe::Comune) {
                    dettaglio.sensitivity = classe == Classe::Personale ? Param::Sensitivity::Personal : Param::Sensitivity::Particular;
                    dettaglio.hidden = e.messaggio_senza_valore;
                }
                out.problemi.push_back(problema(Check::SchemaViolation, f.file, e.puntatore, {dettaglio}));
            }
            continue;
        }
        converti(f.file, d, out.pacchetto);
        out.versioni[f.file] = static_cast<int>(v);
    }
    return out;
}

}  // namespace kzero::api
