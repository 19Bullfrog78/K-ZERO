// K-Zero API - lettura di un pacchetto dati (vedi lettore.hpp e docs/formato-dati.md).
#include "lettore.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#if defined(__unix__) || defined(__APPLE__)
#define KZERO_LETTURA_POSIX 1
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "cassaforte.hpp"
#include "controlli_catalogo.hpp"
#include "formato_dati.hpp"
#include "registro.hpp"
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
        // is_directory segue i collegamenti e non apre nulla: una "pipe" o un dispositivo non bloccano.
        v.push_back({it->path().filename().string(), it->is_directory(ec_tipo)});
    }
    std::sort(v.begin(), v.end(), [](const Voce& a, const Voce& b) { return a.nome < b.nome; });
    return v;
}

namespace {

// Il percorso "dentro" (gia' risolto, senza collegamenti) sta nella cartella "base" (anche lei risolta)?
bool dentro(const std::filesystem::path& percorso, const std::filesystem::path& base) {
    auto p = percorso.begin();
    for (auto b = base.begin(); b != base.end(); ++b, ++p) {
        if (b->empty()) continue;   // "/" finale della cartella
        if (p == percorso.end() || *p != *b) return false;
    }
    return true;
}

// Legge al massimo "massimo" byte da una funzione che riempie un pezzo di memoria (read del sistema o di un
// flusso): si chiede sempre al piu' quello che manca al massimo piu' uno, cosi' un file che cresce o che non
// finisce mai si ferma subito dopo il massimo.
template <class Leggi>
FileLetto leggi_al_massimo(std::uintmax_t massimo, Leggi leggi_pezzo) {
    FileLetto out;
    std::string testo;
    char pezzo[64 * 1024];
    for (;;) {
        const std::uintmax_t manca = massimo + 1 - testo.size();
        const std::size_t chiesti = static_cast<std::size_t>(std::min<std::uintmax_t>(sizeof pezzo, manca));
        const long long letti = leggi_pezzo(pezzo, chiesti);
        if (letti < 0) return out;   // Illeggibile
        if (letti == 0) break;
        testo.append(pezzo, static_cast<std::size_t>(letti));
        if (testo.size() > massimo) {
            out.esito = FileLetto::Esito::TroppoGrande;
            return out;
        }
    }
    out.esito = FileLetto::Esito::Letto;
    out.testo = std::move(testo);
    return out;
}

// Legge un file che deve essere un file normale (percorso gia' risolto o da seguire), al massimo "massimo" byte: la parte
// comune della lettura blindata dei file del pacchetto e di quella dei file che arrivano da fuori (K-019).
FileLetto leggi_file_normale(const std::filesystem::path& vero, std::uintmax_t massimo) {
    namespace fs = std::filesystem;
    FileLetto out;
    std::error_code ec;
    const fs::file_status tipo = fs::status(vero, ec);
    if (ec) return out;
    if (!fs::is_regular_file(tipo)) {
        out.esito = FileLetto::Esito::NonNormale;
        return out;
    }
#if defined(KZERO_LETTURA_POSIX)
    // O_NONBLOCK: se nel frattempo al posto del file e' comparsa una "pipe", l'apertura non resta in attesa.
    const int fd = ::open(vero.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return out;
    struct stat st {};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return out;
    }
    if (!S_ISREG(st.st_mode)) {   // quello che e' stato aperto davvero
        ::close(fd);
        out.esito = FileLetto::Esito::NonNormale;
        return out;
    }
    if (st.st_size > 0 && static_cast<std::uintmax_t>(st.st_size) > massimo) {   // non serve leggerlo
        ::close(fd);
        out.esito = FileLetto::Esito::TroppoGrande;
        return out;
    }
    out = leggi_al_massimo(massimo, [fd](char* dove, std::size_t quanti) -> long long {
        for (;;) {
            const ssize_t n = ::read(fd, dove, quanti);
            if (n >= 0) return static_cast<long long>(n);
            if (errno != EINTR) return -1;
        }
    });
    ::close(fd);
    return out;
#else
    const std::uintmax_t dichiarata = fs::file_size(vero, ec);
    if (!ec && dichiarata > massimo) {
        out.esito = FileLetto::Esito::TroppoGrande;
        return out;
    }
    std::ifstream f(vero, std::ios::binary);
    if (!f) return out;
    return leggi_al_massimo(massimo, [&f](char* dove, std::size_t quanti) -> long long {
        f.read(dove, static_cast<std::streamsize>(quanti));
        if (f.bad()) return -1;
        return static_cast<long long>(f.gcount());
    });
#endif
}

}  // namespace

FileLetto leggi_dal_disco(const std::filesystem::path& cartella, const std::string& nome, std::uintmax_t massimo) {
    namespace fs = std::filesystem;
    FileLetto out;
    const fs::path percorso = cartella / nome;
    std::error_code ec;
    const fs::file_status proprio = fs::symlink_status(percorso, ec);
    if (ec) return out;
    fs::path vero = percorso;
    if (fs::is_symlink(proprio)) {
        // Un collegamento si segue fino in fondo; il file vero deve stare dentro la cartella del pacchetto, risolta
        // anche lei (puo' essere a sua volta un collegamento, es. una cartella sincronizzata).
        if (!fs::exists(percorso, ec)) {
            out.esito = ec ? FileLetto::Esito::Illeggibile : FileLetto::Esito::CollegamentoRotto;
            return out;
        }
        vero = fs::canonical(percorso, ec);
        if (ec) return out;
        const fs::path base = fs::canonical(cartella, ec);
        if (ec) return out;
        if (!dentro(vero, base)) {
            out.esito = FileLetto::Esito::FuoriCartella;
            return out;
        }
    }
    return leggi_file_normale(vero, massimo);
}

FileLetto leggi_file_esterno(const std::filesystem::path& file, std::uintmax_t massimo) {
    namespace fs = std::filesystem;
    FileLetto out;
    std::error_code ec;
    const fs::file_status proprio = fs::symlink_status(file, ec);
    if (ec) return out;
    fs::path vero = file;
    if (fs::is_symlink(proprio)) {
        // Un collegamento si segue fino in fondo: deve portare a un file normale (qui senza il vincolo della cartella, perche'
        // il file lo nomina l'utente e non arriva dentro un pacchetto preparato da altri).
        if (!fs::exists(file, ec)) {
            out.esito = ec ? FileLetto::Esito::Illeggibile : FileLetto::Esito::CollegamentoRotto;
            return out;
        }
        vero = fs::canonical(file, ec);
        if (ec) return out;
    }
    return leggi_file_normale(vero, massimo);
}

FileLetto CartellaSuDisco::leggi(const std::string& nome, std::uintmax_t massimo) const {
    return leggi_dal_disco(cartella_, nome, massimo);
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

FileLetto PacchettoIncorporato::leggi(const std::string& nome, std::uintmax_t massimo) const {
    FileLetto out;
    const auto testo = risorse::trova(cartella_ + "/" + nome);
    if (!testo) return out;
    if (testo->size() > massimo) {
        out.esito = FileLetto::Esito::TroppoGrande;
        return out;
    }
    out.esito = FileLetto::Esito::Letto;
    out.testo = std::string(*testo);
    return out;
}

std::vector<Sorgente::Voce> PacchettoInMemoria::voci() const {
    std::vector<Voce> v;
    for (const auto& [nome, testo] : file) v.push_back({nome, false});
    return v;
}

FileLetto PacchettoInMemoria::leggi(const std::string& nome, std::uintmax_t massimo) const {
    FileLetto out;
    const auto it = file.find(nome);
    if (it == file.end()) return out;
    if (it->second.size() > massimo) {
        out.esito = FileLetto::Esito::TroppoGrande;
        return out;
    }
    out.esito = FileLetto::Esito::Letto;
    out.testo = it->second;
    return out;
}

bool Lettura::forma_valida() const {
    // Gli avvisi (es. un file non riconosciuto, dati in chiaro) non fermano i controlli sul contenuto.
    return std::all_of(problemi.begin(), problemi.end(),
                       [](const Problem& p) { return definizione_controllo(p.check).livello == Livello::Avviso; });
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
    bool numero_enorme = false;                 // un numero troppo grande anche per un numero con decimali (es. 1e400)
};

// Riga e colonna del byte "fine" del testo (colonne contate in caratteri UTF-8).
std::pair<int, int> riga_e_colonna(const std::string& testo, std::size_t fine) {
    int riga = 1, colonna = 1;
    for (std::size_t i = 0; i < fine && i < testo.size(); ++i) {
        if (testo[i] == '\n') {
            ++riga;
            colonna = 1;
        } else if ((static_cast<unsigned char>(testo[i]) & 0xC0) != 0x80) {
            ++colonna;
        }
    }
    return {riga, colonna};
}

// Il primo numero del JSON (fuori dai testi tra virgolette) che non sta in un numero con decimali, es. 1e400: la
// libreria JSON lo rifiuta senza dire dove. Restituisce la sua posizione nel testo, o la fine se non c'e'.
std::size_t numero_enorme(const std::string& t) {
    bool in_testo = false;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        if (in_testo) {
            if (c == '\\') ++i;
            else if (c == '"') in_testo = false;
            continue;
        }
        if (c == '"') {
            in_testo = true;
            continue;
        }
        if (c != '-' && (c < '0' || c > '9')) continue;
        std::size_t fine = i + 1;
        while (fine < t.size() && std::string_view("0123456789+-.eE").find(t[fine]) != std::string_view::npos) ++fine;
        double valore = 0.0;
        const char* inizio = t.data() + i + (c == '-' ? 1 : 0);
        const auto [p, errore] = std::from_chars(inizio, t.data() + fine, valore);
        const std::string_view numero(t.data() + i, fine - i);
        // Fuori misura per eccesso, non per difetto (1e-400 la libreria lo legge come zero).
        if (errore == std::errc::result_out_of_range && numero.find("e-") == std::string_view::npos &&
            numero.find("E-") == std::string_view::npos)
            return i;
        i = fine - 1;
    }
    return t.size();
}

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
        std::tie(a.riga, a.colonna) = riga_e_colonna(testo, e.byte > 0 ? e.byte - 1 : 0);
    } catch (const JsonDati::out_of_range&) {
        // Un numero troppo grande (es. 1e400): un errore nei dati, non un guasto del programma (trovato dal fuzzing).
        a.documento.reset();
        a.numero_enorme = true;
        std::tie(a.riga, a.colonna) = riga_e_colonna(testo, numero_enorme(testo));
    } catch (const JsonDati::exception&) {
        // Qualunque altro rifiuto della libreria JSON: il file non si usa (nessun caso noto).
        a.documento.reset();
        a.riga = 1;
        a.colonna = 1;
    }
    return a;
}

// --- Codifica: il JSON e' sempre UTF-8 (RFC 8259) -----------------------------------------------------

struct CodificaSbagliata {
    bool utf16 = false;     // file salvato in UTF-16 (il "Unicode" del Blocco note)
    int riga = 0, colonna = 0;   // primo carattere non UTF-8 (solo se non e' UTF-16)
};

// Lunghezza della sequenza UTF-8 valida che comincia in t[i], oppure 0.
std::size_t sequenza_utf8(const std::string& t, std::size_t i) {
    const auto b = [&](std::size_t k) { return static_cast<unsigned char>(t[k]); };
    const unsigned char c = b(i);
    if (c < 0x80) return 1;
    std::size_t n = 0;
    unsigned cp = 0, minimo = 0;
    if ((c & 0xE0) == 0xC0) { n = 2; cp = c & 0x1Fu; minimo = 0x80; }
    else if ((c & 0xF0) == 0xE0) { n = 3; cp = c & 0x0Fu; minimo = 0x800; }
    else if ((c & 0xF8) == 0xF0) { n = 4; cp = c & 0x07u; minimo = 0x10000; }
    else return 0;
    if (i + n > t.size()) return 0;
    for (std::size_t k = 1; k < n; ++k) {
        if ((b(i + k) & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (b(i + k) & 0x3Fu);
    }
    if (cp < minimo || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    return n;
}

// Il testo non e' UTF-8? Il controllo viene prima della lettura del JSON: un carattere in un'altra codifica
// (es. un file salvato come "ANSI") farebbe pensare a una virgola mancante.
std::optional<CodificaSbagliata> codifica_sbagliata(const std::string& t) {
    const auto b = [&](std::size_t k) { return static_cast<unsigned char>(t[k]); };
    if (t.size() >= 2 && ((b(0) == 0xFF && b(1) == 0xFE) || (b(0) == 0xFE && b(1) == 0xFF) || b(0) == 0 || b(1) == 0))
        return CodificaSbagliata{true, 0, 0};
    int riga = 1, colonna = 1;
    for (std::size_t i = 0; i < t.size();) {
        const std::size_t n = sequenza_utf8(t, i);
        if (n == 0) return CodificaSbagliata{false, riga, colonna};
        if (t[i] == '\n') {
            ++riga;
            colonna = 1;
        } else {
            ++colonna;
        }
        i += n;
    }
    return std::nullopt;
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
        if (!d) throw std::logic_error("data non valida dopo il controllo dello schema");   // senza il valore: e' un dato
        return *d;
    }
    static std::optional<Day> data_opz(const JsonDati& o, const char* k) {
        const auto it = o.find(k);
        if (it == o.end()) return std::nullopt;
        return data(*it);
    }
    static Minutes orario(const JsonDati& v) {
        const auto m = minutes_from_text(v.get<std::string>());
        if (!m) throw std::logic_error("orario non valido dopo il controllo dello schema");
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
    throw std::logic_error("valore di un elenco fisso non previsto dallo schema");
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
    {"ncc", ServiceKind::Ncc}, {"altro", ServiceKind::Other}};
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

// Fonte con il documento obbligatorio e il punto facoltativo (norme, festivita').
RuleSource fonte_documento(const JsonDati& f) {
    return RuleSource{f.at("documento").get<std::string>(), Conversione::testo(f, "punto"), Conversione::testo(f, "riferimenti")};
}

std::vector<Ref> riferimenti(const JsonDati& o, const char* k, const std::string& p, const Conversione& c) {
    std::vector<Ref> v;
    const auto& e = Conversione::elenco(o, k);
    for (std::size_t i = 0; i < e.size(); ++i) v.push_back({e[i].get<std::string>(), c.src(indice(p + "/" + k, i))});
    return v;
}

// --- Orari di lavoro (K-011) -------------------------------------------------------------------------

TimeCategory categoria_tempo(const JsonDati& v) {
    return valore_di<TimeCategory>({{"guida", TimeCategory::Driving}, {"altre_mansioni", TimeCategory::OtherWork},
                                    {"disponibilita", TimeCategory::Availability}, {"reperibilita", TimeCategory::OnCall},
                                    {"interruzione", TimeCategory::Break}},
                                   v.get<std::string>());
}

std::vector<TimeCategory> categorie_tempo(const JsonDati& o, const char* k) {
    std::vector<TimeCategory> v;
    for (const auto& x : Conversione::elenco(o, k)) v.push_back(categoria_tempo(x));
    return v;
}

const std::vector<std::pair<const char*, RuleFamily>> kFamiglie = {
    {"guida", RuleFamily::Driving}, {"riposi", RuleFamily::Rest}, {"lavoro", RuleFamily::Work}, {"notturno", RuleFamily::Night},
    {"giornate", RuleFamily::Days}};
const std::vector<std::pair<const char*, RuleType>> kTipiRegola = {
    {"conteggio_lavoro", RuleType::WorkCount},          {"lavoro_massimo", RuleType::MaxWork},
    {"guida_continua", RuleType::ContinuousDriving},    {"guida_massima", RuleType::MaxDriving},
    {"lavoro_continuo", RuleType::ContinuousWork},      {"riposo_giornaliero", RuleType::DailyRest},
    {"riposo_settimanale", RuleType::WeeklyRest},       {"lavoro_notturno", RuleType::NightWork},
    {"giornate_massime", RuleType::MaxDays},            {"ampiezza_massima", RuleType::MaxSpan},
    {"giorni_consecutivi", RuleType::ConsecutiveDays}};
const std::vector<std::pair<const char*, RulePeriod>> kPeriodi = {
    {"giornata", RulePeriod::Workday}, {"24_ore", RulePeriod::Day24},          {"settimana", RulePeriod::Week},
    {"due_settimane", RulePeriod::TwoWeeks}, {"mese", RulePeriod::Month}, {"tra_due_riposi", RulePeriod::BetweenRests}};
const std::vector<std::pair<const char*, EmploymentKind>> kRapporti = {
    {"titolare", EmploymentKind::Owner}, {"collaboratore_familiare", EmploymentKind::FamilyHelper}, {"socio", EmploymentKind::Partner},
    {"dipendente_tempo_pieno", EmploymentKind::FullTime}, {"dipendente_part_time", EmploymentKind::PartTime},
    {"a_chiamata", EmploymentKind::OnCall}, {"tempo_determinato", EmploymentKind::FixedTerm}, {"stagionale", EmploymentKind::Seasonal},
    {"autonomo", EmploymentKind::SelfEmployed}, {"volontario", EmploymentKind::Volunteer}, {"altro", EmploymentKind::Other}};
const std::vector<std::pair<const char*, ShiftRole>> kRuoliTurno = {
    {"autista", ShiftRole::Driver}, {"secondo_autista", ShiftRole::SecondDriver}, {"assistente", ShiftRole::Escort}};

// Una condizione del regime con i valori certi e quelli da confermare; tabella = l'elenco fisso dei valori (nullptr: testi).
template <class T>
Applies<T> condizione_regime(const JsonDati& o, const char* k, const std::vector<std::pair<const char*, T>>* tabella) {
    Applies<T> a;
    const auto it = o.find(k);
    if (it == o.end()) return a;
    a.present = true;
    for (const auto& [campo, out] : {std::pair<const char*, std::vector<T>*>{"certi", &a.certain}, {"da_confermare", &a.to_confirm}})
        for (const auto& v : Conversione::elenco(*it, campo)) {
            if constexpr (std::is_same_v<T, std::string>) out->push_back(v.template get<std::string>());
            else out->push_back(valore_di(*tabella, v.template get<std::string>()));
        }
    return a;
}

// I parametri di una lettura di una regola. Il campo "categorie" vale per il conteggio, "interrompono" per la guida continua.
RuleParams parametri_regola(const JsonDati& o, RuleType tipo) {
    using C = Conversione;
    RuleParams r;
    r.measure_name = C::testo(o, "nome_misura");
    r.categories = categorie_tempo(o, tipo == RuleType::ContinuousDriving ? "interrompono" : "categorie");
    r.stops_up_to = C::intero(o, "soste_fino_a_minuti");
    const auto quota = [&](const char* k) -> std::optional<IdleShare> {
        if (!o.contains(k)) return std::nullopt;
        return IdleShare{categorie_tempo(o.at(k), "categorie"), o.at(k).at("percento").get<int>()};
    };
    r.idle_away = quota("inoperoso_fuori_residenza");
    r.idle_home = quota("inoperoso_in_residenza");
    r.max_minutes = C::intero(o, "massimo_minuti");
    r.normal_minutes = C::intero(o, "normale_minuti");
    if (const auto it = o.find("media"); it != o.end()) {
        r.average_weeks = C::intero(*it, "settimane");
        r.average_months = C::intero(*it, "mesi");
        r.average_days = C::intero(*it, "giorni");
    }
    if (const auto it = o.find("estensione"); it != o.end()) {
        r.extension_max = it->at("massimo_minuti").get<int>();
        r.extension_times = it->at("volte_a_settimana").get<int>();
    }
    for (const auto& z : C::elenco(o, "azzera_con")) {
        BreakPattern b;
        for (const auto& m : z.at("parti_minuti")) b.parts.push_back(m.get<int>());
        b.occasional_only = C::vero(z, "solo_servizi_occasionali");
        r.resets.push_back(b);
    }
    if (const auto it = o.find("interruzioni_brevi_contate"); it != o.end()) {
        r.short_counted_up_to = it->at("fino_a_minuti").get<int>();
        r.short_counted_count = it->at("quante").get<int>();
    }
    r.terminus_stops_over = C::intero(o, "eccezione_soste_capolinea_oltre_minuti");
    r.fraction_min = C::intero(o, "frazione_minima_minuti");
    for (const auto& z : C::elenco(o, "pause")) r.pauses.push_back({z.at("lavoro_oltre_minuti").get<int>(), z.at("pausa_minuti").get<int>()});
    r.min_minutes = C::intero(o, "minimo_minuti");
    if (const auto it = o.find("frazionato"); it != o.end())
        for (const auto& m : it->at("parti_minuti")) r.split.push_back(m.get<int>());
    if (const auto it = o.find("ridotto"); it != o.end()) {
        r.reduced_min = it->at("minimo_minuti").get<int>();
        r.reduced_times = C::intero(*it, "volte");
    }
    if (const auto it = o.find("media_riposo"); it != o.end()) {
        r.rest_average_min = it->at("minimo_minuti").get<int>();
        r.rest_average_days = it->at("giorni").get<int>();
        r.rest_average_exclude_from = C::intero(*it, "esclusi_riposi_di_almeno_minuti");
        for (const auto& z : C::elenco(*it, "salvo_interruzioni")) r.rest_average_except.push_back({z.at("minuti").get<int>(), z.at("quante").get<int>()});
    }
    if (const auto it = o.find("reperibilita_interrompe"); it != o.end()) r.on_call_interrupts = it->get<bool>();
    r.two_weeks_regular = C::intero(o, "regolari_in_due_settimane");
    r.within_24h_periods = C::intero(o, "entro_periodi_di_24_ore");
    r.cumulation_weeks = C::intero(o, "cumulo_settimane");
    if (const auto it = o.find("compensazione"); it != o.end()) {
        r.compensation_weeks = it->at("entro_settimane").get<int>();
        r.compensation_attached = it->at("attaccata_a_riposo_minuti").get<int>();
    }
    if (const auto it = o.find("fascia"); it != o.end()) {
        r.band_from = C::orario(it->at("dalle"));
        r.band_to = C::orario(it->at("alle"));
    }
    r.max_days = C::intero(o, tipo == RuleType::ConsecutiveDays ? "massimo_giorni" : "massimo_giornate");
    r.years = C::intero(o, "anni");
    return r;
}

WorkRule regola_orari(const JsonDati& o, const std::string& p, const Conversione& c) {
    WorkRule w;
    w.code = o.at("codice").get<std::string>();
    w.type = valore_di(kTipiRegola, o.at("tipo").get<std::string>());
    w.family = valore_di(kFamiglie, o.at("famiglia").get<std::string>());
    if (o.contains("periodo")) w.period = valore_di(kPeriodi, o.at("periodo").get<std::string>());
    if (o.contains("misura"))
        w.measure = valore_di<RestMeasure>({{"24_ore_dall_inizio_della_giornata", RestMeasure::From24hStart},
                                            {"24_ore_dalla_fine_del_riposo", RestMeasure::From24hEndRest},
                                            {"tra_due_giornate", RestMeasure::BetweenWorkdays}},
                                           o.at("misura").get<std::string>());
    w.certain = parametri_regola(o, w.type);
    // La lettura prudente: gli stessi campi della regola, con quelli scritti in "prudente" al loro posto.
    if (const auto it = o.find("prudente"); it != o.end()) {
        JsonDati unita = o;
        unita.erase("prudente");
        for (const auto& [k, v] : it->items())
            if (k != "nota") unita[k] = v;
        w.prudent = parametri_regola(unita, w.type);
        w.prudent_note = c.testo(*it, "nota");
    }
    if (const auto it = o.find("straordinario"); it != o.end())
        w.overtime = Overtime{it->at("denuncia").get<std::string>(), it->at("ricorrente").get<std::string>(), c.vero(*it, "ricorrente_da_confermare")};
    w.source = fonte_documento(o.at("fonte"));
    w.to_confirm = c.vero(o, "da_confermare");
    w.note = c.testo(o, "nota");
    w.src = c.src(p);
    return w;
}

Regime regime(const JsonDati& o, const std::string& p, const Conversione& c) {
    Regime r;
    r.code = o.at("codice").get<std::string>();
    r.name = o.at("nome").get<std::string>();
    r.kind = o.at("tipo").get<std::string>() == "legge" ? RegimeKind::Law : RegimeKind::CollectiveAgreement;
    r.source = fonte_documento(o.at("fonte"));
    r.valid_from = c.data_opz(o, "valido_dal");
    r.valid_to = c.data_opz(o, "valido_al");
    r.to_confirm = c.vero(o, "da_confermare");
    r.note = c.testo(o, "nota");
    if (const auto it = o.find("si_applica"); it != o.end()) {
        const JsonDati& a = *it;
        r.roles = condizione_regime(a, "ruoli", &kRuoliTurno);
        r.employments = condizione_regime(a, "rapporti", &kRapporti);
        r.agreements = condizione_regime<std::string>(a, "contratti_collettivi", nullptr);
        r.service_kinds = condizione_regime(a, "tipi_servizio", &kTipiServizio);
        if (const auto m = a.find("mezzi"); m != a.end())
            r.vehicle = VehicleCondition{m->at("persone_trasportabili_oltre").get<int>(), c.vero(*m, "da_confermare")};
        if (const auto m = a.find("percorso"); m != a.end())
            r.route = RouteCondition{c.decimale(*m, "fino_a_km"), c.decimale(*m, "oltre_km"), c.vero(*m, "da_confermare")};
        r.like = c.ref_opz(a, "come_regime", p + "/si_applica");
        if (const auto e = a.find("estensione_come_regime"); e != a.end()) r.like_extension_to_confirm = e->get<std::string>() != "vale";
    }
    const auto& escl = c.elenco(o, "esclusioni");
    for (std::size_t k = 0; k < escl.size(); ++k) {
        RegimeExclusion e;
        e.kinds = valori_di(kTipiServizio, escl[k], "tipi_servizio");
        e.together_with = valori_di(kTipiServizio, escl[k], "insieme_a");   // K-011 aggiunta 1
        e.route_up_to_km = c.decimale(escl[k], "percorso_fino_a_km");
        e.to_confirm = c.vero(escl[k], "da_confermare");
        if (escl[k].contains("fonte")) e.source = fonte_documento(escl[k].at("fonte"));
        e.note = c.testo(escl[k], "nota");
        e.src = c.src(indice(p + "/esclusioni", k));
        r.exclusions.push_back(e);
    }
    if (const auto it = o.find("guida_delle_corse_escluse"); it != o.end()) {   // K-011 aggiunta 1
        ExcludedDriving g;
        g.certain = categoria_tempo(it->at("conta_come"));
        g.prudent = it->contains("prudente") ? categoria_tempo(it->at("prudente")) : g.certain;
        g.source = fonte_documento(it->at("fonte"));
        g.note = c.testo(*it, "nota");
        g.src = c.src(p + "/guida_delle_corse_escluse");
        r.excluded_driving = g;
    }
    if (const auto it = o.find("servizi_occasionali"); it != o.end()) {
        if (it->at("tipi_servizio").is_array()) {
            // Versione 2: un solo "da confermare" per tutto l'elenco.
            r.occasional.present = true;
            (c.vero(*it, "da_confermare") ? r.occasional.to_confirm : r.occasional.certain) =
                valori_di(kTipiServizio, *it, "tipi_servizio");
        } else {
            // Versione 3 (K-012): tipo per tipo, certi o da confermare, con la fonte.
            r.occasional = condizione_regime(*it, "tipi_servizio", &kTipiServizio);
            if (it->contains("fonte")) r.occasional_source = fonte_documento(it->at("fonte"));
            r.occasional_note = c.testo(*it, "nota");
        }
    }
    if (const auto it = o.find("estensione"); it != o.end()) {
        r.week_extension = true;
        r.extension_families = valori_di(kFamiglie, *it, "famiglie");
    }
    const auto& esc = c.elenco(o, "esclude");
    for (std::size_t k = 0; k < esc.size(); ++k) {
        const std::string pe = indice(p + "/esclude", k);
        r.overrides.push_back({c.ref(esc[k], "regime", pe), valori_di(kFamiglie, esc[k], "famiglie"),
                               esc[k].at("effetto").get<std::string>() == "esclude" ? ExclusionEffect::Exclude : ExclusionEffect::ToConfirm,
                               c.src(pe)});
    }
    const auto& casi = c.elenco(o, "casi_non_trattati");
    for (std::size_t k = 0; k < casi.size(); ++k)
        r.untreated.push_back({UntreatedCase::MultiManning, fonte_documento(casi[k].at("fonte")), c.testo(casi[k], "nota"),
                               c.src(indice(p + "/casi_non_trattati", k))});
    const auto& regole = c.elenco(o, "regole");
    for (std::size_t k = 0; k < regole.size(); ++k) r.rules.push_back(regola_orari(regole[k], indice(p + "/regole", k), c));
    r.src = c.src(p);
    return r;
}

ChoiceStrength forza(const JsonDati& v) { return v.get<std::string>() == "limite" ? ChoiceStrength::Limit : ChoiceStrength::Preference; }

std::vector<WorkChoice> scelte_orari(const JsonDati& o, const std::string& p, const Conversione& c) {
    std::vector<WorkChoice> out;
    const auto& e = c.elenco(o, "scelte_orari");
    for (std::size_t k = 0; k < e.size(); ++k) {
        WorkChoice w;
        w.type = valore_di(kTipiRegola, e[k].at("tipo").get<std::string>());
        if (e[k].contains("periodo")) w.period = valore_di(kPeriodi, e[k].at("periodo").get<std::string>());
        w.value = c.intero(e[k], w.type == RuleType::DailyRest         ? "minimo_minuti"
                                 : w.type == RuleType::ConsecutiveDays ? "massimo_giorni"
                                                                       : "massimo_minuti")
                      .value_or(0);
        w.strength = forza(e[k].at("forza"));
        w.note = c.testo(e[k], "nota");
        w.src = c.src(indice(p + "/scelte_orari", k));
        out.push_back(w);
    }
    return out;
}

// Modo delle corse (K-011 aggiunta 1): OUT o tachigrafo per tipo di servizio, per servizio e per corsa, con la forzatura e la
// dichiarazione.
TripModes modo_corse(const JsonDati& o, const std::string& p, const Conversione& c) {
    TripModes m;
    m.regime = c.ref(o, "regime", p);
    if (o.contains("tipi_diversi"))
        m.mixed = valore_di<MixedModes>({{"piu_prudente", MixedModes::MostPrudent}, {"out", MixedModes::Out}, {"tachigrafo", MixedModes::Tachograph}},
                                        o.at("tipi_diversi").get<std::string>());
    if (o.contains("forza")) m.strength = forza(o.at("forza"));
    m.note = c.testo(o, "nota");
    m.src = c.src(p);
    // Motivo e chi sono dati personali (nella dichiarazione, dato particolare, anche legati): chi non li riceve dalla
    // cassaforte li ha mancanti, e si mostrano come "riservato" (K-011 aggiunta 2).
    const auto giustificazione = [&](const JsonDati& g, const std::string& pg) {
        ModeJustification j;
        j.reason = c.testo(g, "motivo");
        j.who = c.testo(g, "chi");
        j.when = c.data(g.at("quando"));
        j.until = c.data_opz(g, "fino_al");
        j.src = c.src(pg);
        return j;
    };
    const auto voci = [&](const char* campo, std::vector<ModeEntry>& out) {
        const auto& e = c.elenco(o, campo);
        for (std::size_t k = 0; k < e.size(); ++k) {
            const std::string pe = indice(p + "/" + campo, k);
            ModeEntry x;
            x.mode = e[k].at("modo").get<std::string>() == "tachigrafo" ? TripModeKind::Tachograph : TripModeKind::Out;
            if (e[k].contains("tipo_servizio")) x.kind = valore_di(kTipiServizio, e[k].at("tipo_servizio").get<std::string>());
            x.service = c.ref_opz(e[k], "servizio", pe);
            x.trip = c.ref_opz(e[k], "corsa", pe);
            if (e[k].contains("forza")) x.strength = forza(e[k].at("forza"));
            if (e[k].contains("forzatura")) x.forcing = giustificazione(e[k].at("forzatura"), pe + "/forzatura");
            if (e[k].contains("dichiarazione")) x.declaration = giustificazione(e[k].at("dichiarazione"), pe + "/dichiarazione");
            x.note = c.testo(e[k], "nota");
            x.src = c.src(pe);
            out.push_back(x);
        }
    };
    voci("per_tipo_servizio", m.by_kind);
    voci("servizi", m.by_service);
    voci("corse", m.by_trip);
    return m;
}

ReturnKey rientro(const JsonDati& o, Source src) {
    ReturnKey k;
    k.min_stop = Conversione::intero(o, "fermo_minimo_minuti");
    k.driving_factor = Conversione::decimale(o, "fattore_guida");
    k.max_empty_km = Conversione::decimale(o, "km_vuoti_massimi");
    if (o.contains("forza")) k.strength = forza(o.at("forza"));
    k.src = std::move(src);
    return k;
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
    m.door_to_door_sensitive = c.vero(d.at("impostazioni"), "porta_a_porta_sensibile");   // versione 2
    if (const auto v = d.find("cassaforte"); v != d.end() && v->is_object()) {               // versione 2
        m.vault_id = v->at("id").get<std::string>();
        m.vault_generation = v->at("generazione").get<int>();
    }
    const auto& falsi = c.elenco(d, "falsi_allarmi");                                       // versione 3 (K-018)
    for (std::size_t i = 0; i < falsi.size(); ++i) {
        const auto& f = falsi[i];
        FalseAlarm x;
        x.check = f.at("controllo").get<std::string>();
        x.file = f.at("file").get<std::string>();
        x.field = f.at("campo").get<std::string>();
        x.element = c.testo(f, "elemento");
        x.text = f.at("testo").get<std::string>();
        x.reason = f.at("motivo").get<std::string>();
        x.when = c.data(f.at("quando"));
        x.src = c.src(indice("/falsi_allarmi", i));
        m.false_alarms.push_back(x);
    }
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

void aziende(const JsonDati& d, Package& pk) {
    const Conversione c("aziende.json");
    const auto& v = d.at("aziende");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& a = v[i];
        const std::string p = indice("/aziende", i);
        CompanySettings x;
        x.company = c.ref(a, "azienda", p);
        x.norms = c.ref_opz(a, "norme", p);
        const auto& imp = a.at("impostazioni");
        x.shelter = valore_di<ShelterNearHome>({{"preferito", ShelterNearHome::Preferred}, {"ammesso", ShelterNearHome::Allowed},
                                                {"mai", ShelterNearHome::Never}},
                                               imp.at("ricovero_vicino_a_casa").get<std::string>());
        x.pairing = valore_di<DriverVehiclePairing>({{"nessuno", DriverVehiclePairing::None}, {"preferito", DriverVehiclePairing::Preferred},
                                                     {"fisso", DriverVehiclePairing::Fixed}},
                                                    imp.at("abbinamento_autista_mezzo").get<std::string>());
        const auto& att = c.elenco(a, "attivita");
        for (std::size_t k = 0; k < att.size(); ++k) {
            ActivityType y;
            y.code = att[k].at("codice").get<std::string>();
            y.name = att[k].at("nome").get<std::string>();
            y.uses_vehicle = att[k].at("usa_il_mezzo").get<bool>();
            if (att[k].contains("tempo")) y.time = categoria_tempo(att[k].at("tempo"));   // versione 2
            y.note = c.testo(att[k], "nota");
            y.src = c.src(indice(p + "/attivita", k));
            x.activities.push_back(y);
        }
        // Versione 2 (K-011): orari di lavoro.
        if (const auto it = imp.find("soste_non_scritte"); it != imp.end()) {
            UnwrittenStops u;
            u.category = categoria_tempo(it->at("categoria"));
            u.over_minutes = c.intero(*it, "oltre_minuti");
            if (it->contains("categoria_oltre")) u.category_over = categoria_tempo(it->at("categoria_oltre"));
            u.src = c.src(p + "/impostazioni/soste_non_scritte");
            x.unwritten = u;
        }
        x.days_margin = c.intero(imp, "margine_giornate_massime");
        x.choices = scelte_orari(a, p, c);
        if (a.contains("rientro")) x.return_key = rientro(a.at("rientro"), c.src(p + "/rientro"));
        if (a.contains("modo_corse")) x.trip_modes = modo_corse(a.at("modo_corse"), p + "/modo_corse", c);
        for (const auto& w : c.elenco(a, "parole_sulla_salute")) x.health_words.push_back(w.get<std::string>());   // versione 4
        const auto& spec = c.elenco(a, "speciali_esigenze");
        for (std::size_t k = 0; k < spec.size(); ++k) {
            const std::string ps = indice(p + "/speciali_esigenze", k);
            x.special_needs.push_back({c.ref(spec[k], "regola", ps), spec[k].at("riferimento").get<std::string>(), c.testo(spec[k], "nota"),
                                       c.src(ps)});
        }
        const auto voci = [&](const char* campo, std::vector<ListEntry>& out) {
            const auto& e = c.elenco(a, campo);
            for (std::size_t k = 0; k < e.size(); ++k)
                out.push_back({e[k].at("codice").get<std::string>(), e[k].at("nome").get<std::string>(), c.testo(e[k], "nota"),
                               c.src(indice(p + "/" + campo, k))});
        };
        voci("tipi_assenza", x.absence_types);
        voci("tipi_fermo", x.stop_types);
        voci("competenze", x.competences);
        x.note = c.testo(a, "nota");
        x.src = c.src(p);
        pk.company_settings.push_back(x);
    }
}

void norme(const JsonDati& d, Package& pk) {
    const Conversione c("norme.json");
    const auto& v = d.at("norme");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& n = v[i];
        const std::string p = indice("/norme", i);
        Norms x;
        x.code = n.at("codice").get<std::string>();
        x.name = n.at("nome").get<std::string>();
        x.country = n.at("paese").get<std::string>();
        const auto voci = [&](const char* campo, std::vector<NormEntry>& out) {
            const auto& e = c.elenco(n, campo);
            for (std::size_t k = 0; k < e.size(); ++k) {
                NormEntry y;
                y.code = e[k].at("codice").get<std::string>();
                y.name = e[k].at("nome").get<std::string>();
                if (e[k].contains("fonte")) y.source = fonte_documento(e[k].at("fonte"));
                y.note = c.testo(e[k], "nota");
                y.src = c.src(indice(p + "/" + campo, k));
                out.push_back(y);
            }
        };
        voci("abilitazioni", x.licences);
        voci("usi", x.uses);
        const auto& req = c.elenco(n, "requisiti_guida");
        for (std::size_t k = 0; k < req.size(); ++k) {
            const auto& r = req[k];
            const std::string pr = indice(p + "/requisiti_guida", k);
            DrivingRequirement y;
            y.categories = valori_di(kCategorie, r, "categorie");
            y.min_seats = c.intero(r, "posti_da");
            y.max_seats = c.intero(r, "posti_fino_a");
            if (r.contains("rimorchio")) y.trailer = r.at("rimorchio").get<bool>();
            y.uses = riferimenti(r, "usi", pr, c);
            const auto& alt = r.at("alternative");
            for (std::size_t q = 0; q < alt.size(); ++q) {
                const std::string pa = indice(pr + "/alternative", q);
                y.alternatives.push_back({riferimenti(alt[q], "abilitazioni", pa, c), c.intero(alt[q], "posti_fino_a"),
                                          c.decimale(alt[q], "lunghezza_fino_a_m"), c.src(pa)});
            }
            y.source = fonte_documento(r.at("fonte"));
            y.to_confirm = c.vero(r, "da_confermare");
            y.note = c.testo(r, "nota");
            y.src = c.src(pr);
            x.requirements.push_back(y);
        }
        const auto& usi = c.elenco(n, "usi_per_servizio");
        for (std::size_t k = 0; k < usi.size(); ++k) {
            const std::string pu = indice(p + "/usi_per_servizio", k);
            ServiceUsesNorm y;
            y.value = {valori_di(kTipiServizio, usi[k], "tipi_servizio"), riferimenti(usi[k], "usi", pu, c), c.src(pu)};
            y.source = fonte_documento(usi[k].at("fonte"));
            y.to_confirm = c.vero(usi[k], "da_confermare");
            y.note = c.testo(usi[k], "nota");
            y.src = c.src(pu);
            x.service_uses.push_back(y);
        }
        // Versione 2 (K-011): leggi e contratti collettivi sugli orari, cambi dell'ora.
        const auto& reg = c.elenco(n, "regimi");
        for (std::size_t k = 0; k < reg.size(); ++k) x.regimes.push_back(regime(reg[k], indice(p + "/regimi", k), c));
        if (const auto it = n.find("cambi_ora"); it != n.end()) {
            x.clock_source = fonte_documento(it->at("fonte"));
            const auto& date = it->at("date");
            for (std::size_t k = 0; k < date.size(); ++k)
                x.clock_changes.push_back({c.data(date[k].at("data")), c.orario(date[k].at("da")), c.orario(date[k].at("a")),
                                           c.src(indice(p + "/cambi_ora/date", k))});
        }
        for (const auto& w : c.elenco(n, "parole_sulla_salute")) x.health_words.push_back(w.get<std::string>());   // versione 4 (K-018)
        x.note = c.testo(n, "nota");
        x.src = c.src(p);
        pk.norms.push_back(x);
    }
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
        x.sensitive = c.vero(l, "sensibile");   // versione 2
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
        if (par.contains("usi_dei_mezzi")) {   // versione 2
            const auto& j = par.at("usi_dei_mezzi");
            const std::string pj = pp + "/usi_dei_mezzi";
            ServiceUsesRule u;
            const auto& valori = j.at("valori");
            for (std::size_t k = 0; k < valori.size(); ++k) {
                const std::string pv = indice(pj + "/valori", k);
                u.values.push_back({valori_di(kTipiServizio, valori[k], "tipi_servizio"), riferimenti(valori[k], "usi", pv, c), c.src(pv)});
            }
            u.source = fonte(j.at("fonte"));
            u.src = c.src(pj);
            x.vehicle_uses = u;
        }
        if (par.contains("autorizzazione_del_committente")) {   // versione 2
            const auto& j = par.at("autorizzazione_del_committente");
            x.client_authorization = ClientAuthorizationRule{valori_di(kTipiServizio, j, "tipi_servizio"), fonte(j.at("fonte")),
                                                             c.src(pp + "/autorizzazione_del_committente")};
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

void festivita(const JsonDati& d, Package& pk) {
    const Conversione c("festivita.json");
    const auto& v = d.at("festivita");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& f = v[i];
        const std::string p = indice("/festivita", i);
        HolidayCalendar x;
        x.code = f.at("codice").get<std::string>();
        x.name = f.at("nome").get<std::string>();
        x.country = f.at("paese").get<std::string>();
        x.territory = c.testo(f, "territorio");
        x.source = fonte_documento(f.at("fonte"));
        const auto& date = f.at("date");
        for (std::size_t j = 0; j < date.size(); ++j)
            x.days.push_back({c.data(date[j].at("data")), date[j].at("nome").get<std::string>(), c.vero(date[j], "da_confermare"),
                              c.testo(date[j], "nota"), c.src(indice(p + "/date", j))});
        x.note = c.testo(f, "nota");
        x.src = c.src(p);
        pk.holidays.push_back(x);
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
        // Versione 2: giorni di altri calendari o delle festivita' aggiunti e tolti (la versione 1 non li ha).
        const auto collegamenti = [&](const char* campo, std::vector<CalendarLink>& out) {
            if (!k.contains(campo)) return;
            const auto& o = k.at(campo);
            const std::string pc = p + "/" + campo;
            for (const auto& [chiave, tipo] : {std::pair<const char*, CalendarLinkKind>{"calendari", CalendarLinkKind::Calendar},
                                               {"festivita", CalendarLinkKind::Holidays}}) {
                const auto& e = c.elenco(o, chiave);
                for (std::size_t j = 0; j < e.size(); ++j)
                    out.push_back({tipo, Ref{e[j].get<std::string>(), c.src(indice(pc + "/" + chiave, j))}});
            }
        };
        collegamenti("aggiunge", x.adds);
        collegamenti("toglie", x.removes);
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
        pk.clients.push_back({v[i].at("codice").get<std::string>(), v[i].at("nome").get<std::string>(), c.esterni(v[i]), false, {},
                              c.src(indice("/committenti", i))});
    // Versione 2 (K-012): i committenti privati. Nome e nota sono personali: senza il permesso non arrivano dalla cassaforte.
    const auto& w = c.elenco(d, "committenti_privati");
    for (std::size_t i = 0; i < w.size(); ++i)
        pk.clients.push_back({w[i].at("codice").get<std::string>(), c.testo(w[i], "nome"), c.esterni(w[i]), true, c.testo(w[i], "nota"),
                              c.src(indice("/committenti_privati", i))});
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
        const std::string carrozzina = c.testo(e, "carrozzina");
        x.needs.wheelchair = carrozzina == "sulla_carrozzina" ? WheelchairUse::Seated
                             : carrozzina == "piegata"        ? WheelchairUse::Folded
                                                              : WheelchairUse::None;
        x.needs.folded_aids = c.intero(e, "ausili_piegati").value_or(0);
        x.needs.assistant = c.vero(e, "assistente");
        x.needs.other = c.testo(e, "altro");
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
        // Il tipo del servizio e quello del gruppo sono dati particolari obbligatori: mancano solo a chi non puo' leggerli. K-017:
        // allora non sono noti, mai un altro tipo (prima diventavano "altro" e il piano ne applicava i requisiti).
        x.kind = s.contains("tipo") ? MaybeKnown<ServiceKind>(valore_di(kTipiServizio, s.at("tipo").get<std::string>()))
                                    : MaybeKnown<ServiceKind>::unknown();
        x.kind_description = c.testo(s, "tipo_descrizione");
        x.contract = c.ref(s, "contratto", p);
        x.calendar = c.ref(s, "calendario", p);
        const auto& lib = s.at("liberta_oraria");
        x.freedom = lib.at("tipo").get<std::string>() == "libera" ? TimeFreedom::Free : TimeFreedom::Fixed;
        x.agree_threshold_min = c.intero(lib, "soglia_da_concordare_minuti");
        x.rules = regole(s, p, c);
        x.competences = riferimenti(s, "competenze_richieste", p, c);   // versione 2
        const auto& gruppi = s.at("domanda").at("gruppi");
        for (std::size_t k = 0; k < gruppi.size(); ++k) {
            const auto& g = gruppi[k];
            const std::string pg = indice(p + "/domanda/gruppi", k);
            PassengerGroup y;
            y.code = g.at("codice").get<std::string>();
            y.name = c.testo(g, "nome");
            y.type = g.contains("tipo") ? MaybeKnown<GroupType>(valore_di(kTipiGruppo, g.at("tipo").get<std::string>()))
                                        : MaybeKnown<GroupType>::unknown();
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
                                   c.intero(fermate[f], "ausili_piegati").value_or(0), c.src(pf)});
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

std::vector<Loan> prestiti(const JsonDati& o, const std::string& p, const Conversione& c) {
    std::vector<Loan> v;
    const auto& e = c.elenco(o, "prestiti");
    for (std::size_t k = 0; k < e.size(); ++k) {
        const std::string pk = indice(p + "/prestiti", k);
        v.push_back({c.ref(e[k], "azienda", pk), e[k].at("accordo").get<std::string>(), c.data_opz(e[k], "dal"), c.data_opz(e[k], "al"),
                     c.src(pk)});
    }
    return v;
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
        x.aids_in_trunk = c.intero(m, "ausili_bagagliaio").value_or(0);
        if (m.contains("ausili_tra_i_sedili"))
            x.seats_per_aid_between = m.at("ausili_tra_i_sedili").at("posti_occupati").get<int>();
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
        // Versione 2: uso, autorizzazioni a un uso in piu', vincolo di destinazione, scorta, prestiti.
        x.use = c.ref_opz(m, "uso", p);
        const auto& aut = c.elenco(m, "autorizzazioni_uso");
        for (std::size_t k = 0; k < aut.size(); ++k) {
            const std::string pa = indice(p + "/autorizzazioni_uso", k);
            x.use_authorizations.push_back({c.ref(aut[k], "uso", pa), aut[k].at("ente").get<std::string>(),
                                            aut[k].at("riferimento").get<std::string>(), {c.data(aut[k].at("dal")), c.data(aut[k].at("al"))},
                                            c.testo(aut[k], "prescrizioni"), c.src(pa)});
        }
        if (m.contains("vincolo_destinazione")) {
            const auto& j = m.at("vincolo_destinazione");
            const std::string pv = p + "/vincolo_destinazione";
            DestinationConstraint v;
            v.kinds = valori_di(kTipiServizio, j.at("ammessi"), "tipi_servizio");
            v.contracts = riferimenti(j.at("ammessi"), "contratti", pv + "/ammessi", c);
            v.until = c.data_opz(j, "fino_al");
            v.source = j.at("fonte").get<std::string>();
            v.note = c.testo(j, "nota");
            v.src = c.src(pv);
            x.constraint = v;
        }
        x.spare = c.vero(m, "scorta");
        x.loans = prestiti(m, p, c);
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

std::optional<HoursLimits> limiti_ore(const JsonDati& o, const char* k) {
    if (!o.contains(k)) return std::nullopt;
    const auto& j = o.at(k);
    return HoursLimits{Conversione::decimale(j, "giorno"), Conversione::decimale(j, "settimana"), Conversione::decimale(j, "mese")};
}

void persone(const JsonDati& d, Package& pk) {
    const Conversione c("persone.json");
    const auto& v = d.at("persone");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& s = v[i];
        const std::string p = indice("/persone", i);
        Person x;
        x.code = s.at("codice").get<std::string>();
        x.company = c.ref(s, "azienda", p);
        // Ruoli e rapporto sono dati personali obbligatori: mancano solo a chi non puo' leggerli (cassaforte, K-010).
        for (const auto& r : c.elenco(s, "ruoli")) x.roles.push_back(r.get<std::string>() == "autista" ? PersonRole::Driver : PersonRole::Escort);
        x.name = c.testo(s, "nome");
        if (s.contains("casa")) {
            const auto& h = s.at("casa");
            x.home = Home{c.testo(h, "indirizzo"), c.testo(h, "comune"), c.coordinate_opz(h, "coordinate")};
        }
        x.base_place = c.ref_opz(s, "presa_servizio", p);
        x.usual_vehicle = c.ref_opz(s, "mezzo_abituale", p);
        x.hypothetical = c.vero(s, "ipotetica");
        if (s.contains("rapporto")) {
            const auto& rap = s.at("rapporto");
            x.employment = valore_di<EmploymentKind>(
                {{"titolare", EmploymentKind::Owner}, {"collaboratore_familiare", EmploymentKind::FamilyHelper}, {"socio", EmploymentKind::Partner},
                 {"dipendente_tempo_pieno", EmploymentKind::FullTime}, {"dipendente_part_time", EmploymentKind::PartTime},
                 {"a_chiamata", EmploymentKind::OnCall}, {"tempo_determinato", EmploymentKind::FixedTerm}, {"stagionale", EmploymentKind::Seasonal},
                 {"autonomo", EmploymentKind::SelfEmployed}, {"volontario", EmploymentKind::Volunteer}, {"altro", EmploymentKind::Other}},
                rap.at("tipo").get<std::string>());
            x.employment_description = c.testo(rap, "descrizione");
        } else {
            x.employment = EmploymentKind::Other;
        }
        if (s.contains("contratto_collettivo")) {
            x.agreement_code = s.at("contratto_collettivo").at("codice").get<std::string>();
            x.agreement_name = s.at("contratto_collettivo").at("nome").get<std::string>();
        }
        const auto& ore = c.elenco(s, "ore");
        for (std::size_t k = 0; k < ore.size(); ++k)
            x.hours.push_back({c.data_opz(ore[k], "dal"), c.data_opz(ore[k], "al"), limiti_ore(ore[k], "previste"),
                               limiti_ore(ore[k], "massime"), c.src(indice(p + "/ore", k))});
        const auto& disp = c.elenco(s, "disponibilita");
        for (std::size_t k = 0; k < disp.size(); ++k)
            x.availability.push_back({c.data_opz(disp[k], "dal"), c.data_opz(disp[k], "al"), c.giorni(disp[k].at("giorni")),
                                      c.orario(disp[k].at("dalle")), c.orario(disp[k].at("alle")), c.src(indice(p + "/disponibilita", k))});
        const auto& costi = c.elenco(s, "costo_orario");
        for (std::size_t k = 0; k < costi.size(); ++k)
            x.hourly_cost.push_back({c.data_opz(costi[k], "dal"), c.data_opz(costi[k], "al"), costi[k].at("valore").get<double>(),
                                     c.src(indice(p + "/costo_orario", k))});
        const auto& abil = c.elenco(s, "abilitazioni");
        for (std::size_t k = 0; k < abil.size(); ++k) {
            const std::string pa = indice(p + "/abilitazioni", k);
            x.licences.push_back({c.ref(abil[k], "tipo", pa), c.data_opz(abil[k], "scadenza"), c.testo(abil[k], "riferimento"), c.src(pa)});
        }
        const auto& aut = c.elenco(s, "autorizzazioni");
        for (std::size_t k = 0; k < aut.size(); ++k) {
            const std::string pa = indice(p + "/autorizzazioni", k);
            x.authorizations.push_back({c.ref(aut[k], "committente", pa), c.testo(aut[k], "riferimento"), c.data_opz(aut[k], "scadenza"), c.src(pa)});
        }
        x.competences = riferimenti(s, "competenze", p, c);
        const auto& pref = c.elenco(s, "preferenze");
        for (std::size_t k = 0; k < pref.size(); ++k) {
            const auto& j = pref[k];
            const std::string pp = indice(p + "/preferenze", k);
            Preference y;
            y.kind = valore_di<PreferenceKind>({{"giorni_liberi", PreferenceKind::FreeDays}, {"fascia_oraria", PreferenceKind::TimeWindow},
                                                {"evita_turni", PreferenceKind::AvoidShifts}, {"pausa_massima", PreferenceKind::MaxBreak},
                                                {"altro", PreferenceKind::Other}},
                                               j.at("tipo").get<std::string>());
            y.from = c.data_opz(j, "dal");
            y.to = c.data_opz(j, "al");
            if (j.contains("giorni")) y.days = c.giorni(j.at("giorni"));
            y.start = c.orario_opz(j, "dalle").value_or(0);
            y.end = c.orario_opz(j, "alle").value_or(0);
            y.shifts = riferimenti(j, "turni", pp, c);
            y.max_break_min = c.intero(j, "minuti").value_or(0);
            y.description = c.testo(j, "descrizione");
            y.src = c.src(pp);
            x.preferences.push_back(y);
        }
        x.loans = prestiti(s, p, c);
        // Versione 2 (K-011): scelte sugli orari, chiave del rientro, giornate prima del pacchetto (dati personali).
        x.choices = scelte_orari(s, p, c);
        if (s.contains("rientro")) x.return_key = rientro(s.at("rientro"), c.src(p + "/rientro"));
        if (const auto it = s.find("giornate_a_chiamata"); it != s.end())
            x.prior_days = PriorDays{it->at("giornate").get<int>(), c.data(it->at("contate_dal")), c.src(p + "/giornate_a_chiamata")};
        x.note = c.testo(s, "nota");
        x.src = c.src(p);
        pk.persons.push_back(x);
    }
}

void assenze(const JsonDati& d, Package& pk) {
    const Conversione c("assenze.json");
    const auto& v = d.at("assenze");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/assenze", i);
        pk.absences.push_back({c.ref(v[i], "persona", p), c.ref(v[i], "tipo", p), {c.data(v[i].at("dal")), c.data(v[i].at("al"))},
                               c.orario_opz(v[i], "dalle"), c.orario_opz(v[i], "alle"), c.src(p)});
    }
}

void fermi(const JsonDati& d, Package& pk) {
    const Conversione c("fermi.json");
    const auto& v = d.at("fermi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/fermi", i);
        pk.stops.push_back({c.ref(v[i], "mezzo", p), c.ref(v[i], "tipo", p), {c.data(v[i].at("dal")), c.data(v[i].at("al"))},
                            c.orario_opz(v[i], "dalle"), c.orario_opz(v[i], "alle"), c.testo(v[i], "nota"), c.src(p)});
    }
}

std::optional<TripPoint> punto_corsa(const JsonDati& o, const char* k, const std::string& p, const Conversione& c) {
    if (!o.contains(k)) return std::nullopt;
    const std::string pk = p + "/" + k;
    return TripPoint{c.ref(o.at(k), "luogo", pk), c.orario_opz(o.at(k), "orario"), c.src(pk)};
}

void turni(const JsonDati& d, Package& pk) {
    const Conversione c("turni.json");
    const auto& v = d.at("turni");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& s = v[i];
        const std::string p = indice("/turni", i);
        Shift x;
        x.code = s.at("codice").get<std::string>();
        x.name = c.testo(s, "nome");
        x.company = c.ref(s, "azienda", p);
        x.role = valore_di<ShiftRole>({{"autista", ShiftRole::Driver}, {"assistente", ShiftRole::Escort},
                                       {"secondo_autista", ShiftRole::SecondDriver}},
                                      s.at("ruolo").get<std::string>());
        x.calendar = c.ref(s, "calendario", p);
        const auto& m = s.at("mezzo");
        x.vehicle_mode = valore_di<ShiftVehicleMode>({{"preciso", ShiftVehicleMode::Specific}, {"abituale", ShiftVehicleMode::Usual},
                                                      {"da_assegnare", ShiftVehicleMode::ToAssign}, {"nessuno", ShiftVehicleMode::None}},
                                                     m.at("modo").get<std::string>());
        x.vehicle = c.ref_opz(m, "codice", p + "/mezzo");
        x.vehicle_src = c.src(p + "/mezzo");
        x.start_place = c.ref(s, "inizio", p);
        x.end_place = c.ref(s, "fine", p);
        const auto& el = s.at("elementi");
        for (std::size_t k = 0; k < el.size(); ++k) {
            const auto& e = el[k];
            const std::string pe = indice(p + "/elementi", k);
            ShiftItem y;
            y.src = c.src(pe);
            if (e.contains("corsa")) {
                y.is_trip = true;
                y.trip.trip = c.ref(e, "corsa", pe);
                y.trip.from = punto_corsa(e, "da", pe, c);
                y.trip.to = punto_corsa(e, "a", pe, c);
                if (e.contains("assistente"))
                    y.trip.escort = e.at("assistente").get<std::string>() == "azienda" ? EscortProvider::Company : EscortProvider::Client;
            } else {
                y.is_trip = false;
                y.activity.type = c.ref(e, "attivita", pe);
                y.activity.place = c.ref(e, "luogo", pe);
                y.activity.to_place = c.ref_opz(e, "verso", pe);
                y.activity.start = c.orario(e.at("dalle"));
                y.activity.end = c.orario(e.at("alle"));
                y.activity.empty_km = c.decimale(e, "km_vuoti");
                y.activity.calendar = c.ref_opz(e, "calendario", pe);
                y.activity.note = c.testo(e, "nota");
            }
            x.items.push_back(y);
        }
        x.competences = riferimenti(s, "competenze_richieste", p, c);
        x.note = c.testo(s, "nota");
        x.src = c.src(p);
        pk.shifts.push_back(std::move(x));
    }
}

void rotazioni(const JsonDati& d, Package& pk) {
    const Conversione c("rotazioni.json");
    const auto& v = d.at("rotazioni");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& r = v[i];
        const std::string p = indice("/rotazioni", i);
        Rotation x;
        x.code = r.at("codice").get<std::string>();
        x.name = c.testo(r, "nome");
        x.unit = r.at("passo").get<std::string>() == "mese" ? RotationUnit::Month : RotationUnit::Day;
        x.length = r.at("lunghezza").get<int>();
        x.start = c.data(r.at("partenza"));
        x.from = c.data_opz(r, "dal");
        x.to = c.data_opz(r, "al");
        const auto& righe = r.at("righe");
        for (std::size_t k = 0; k < righe.size(); ++k) {
            const std::string pr = indice(p + "/righe", k);
            RotationRow row;
            row.person = c.ref(righe[k], "persona", pr);
            const auto& passi = righe[k].at("passi");
            for (std::size_t q = 0; q < passi.size(); ++q) {
                const std::string pp = indice(pr + "/passi", q);
                RotationStep st;
                st.src = c.src(pp);
                if (passi[q].is_string()) {
                    st.rest = true;
                } else {
                    for (std::size_t z = 0; z < passi[q].size(); ++z)
                        st.shifts.push_back({passi[q][z].get<std::string>(), c.src(indice(pp, z))});
                }
                row.steps.push_back(st);
            }
            row.src = c.src(pr);
            x.rows.push_back(row);
        }
        x.note = c.testo(r, "nota");
        x.src = c.src(p);
        pk.rotations.push_back(std::move(x));
    }
}

void servizi_fissi(const JsonDati& d, Package& pk) {
    const Conversione c("servizi_fissi.json");
    const auto& v = d.at("servizi_fissi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/servizi_fissi", i);
        pk.fixed.push_back({v[i].at("codice").get<std::string>(), c.ref(v[i], "turno", p), c.ref(v[i], "persona", p),
                            c.ref(v[i], "calendario", p), c.testo(v[i], "nota"), c.src(p)});
    }
}

void assegnazioni(const JsonDati& d, Package& pk) {
    const Conversione c("assegnazioni.json");
    const auto& v = d.at("assegnazioni");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/assegnazioni", i);
        pk.day_assignments.push_back({c.data(v[i].at("data")), c.ref(v[i], "turno", p), c.ref_opz(v[i], "persona", p),
                                      c.ref_opz(v[i], "mezzo", p), c.testo(v[i], "nota"), c.src(p)});
    }
}

void blocchi(const JsonDati& d, Package& pk) {
    const Conversione c("blocchi.json");
    const auto& v = d.at("blocchi");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const std::string p = indice("/blocchi", i);
        Block x;
        x.code = v[i].at("codice").get<std::string>();
        x.vehicle = c.ref(v[i], "mezzo", p);
        x.trips = riferimenti(v[i], "corse", p, c);
        x.from = c.data_opz(v[i], "dal");
        x.to = c.data_opz(v[i], "al");
        x.strength = v[i].at("forza").get<std::string>() == "tutto_fisso" ? BlockStrength::AllFixed : BlockStrength::VehicleFixed;
        x.note = c.testo(v[i], "nota");
        x.src = c.src(p);
        pk.blocks.push_back(x);
    }
}

void converti(const std::string& file, const JsonDati& d, Package& pk) {
    using Funzione = void (*)(const JsonDati&, Package&);
    static const std::vector<std::pair<const char*, Funzione>> conversioni = {
        {"pacchetto.json", manifesto}, {"aziende.json", aziende}, {"norme.json", norme}, {"luoghi.json", luoghi}, {"tratti.json", tratti},
        {"regolamenti.json", regolamenti}, {"festivita.json", festivita}, {"calendari.json", calendari}, {"scuole.json", scuole},
        {"committenti.json", committenti}, {"contratti.json", contratti}, {"gruppi_servizi.json", gruppi_servizi},
        {"servizi.json", servizi}, {"corse.json", corse}, {"mezzi.json", mezzi}, {"rimesse.json", rimesse},
        {"tempi_sosta.json", tempi_sosta}, {"persone.json", persone}, {"assenze.json", assenze}, {"fermi.json", fermi},
        {"turni.json", turni}, {"rotazioni.json", rotazioni}, {"servizi_fissi.json", servizi_fissi},
        {"assegnazioni.json", assegnazioni}, {"blocchi.json", blocchi}};
    for (const auto& [nome, f] : conversioni)
        if (file == nome) return f(d, pk);
    throw std::logic_error("file del formato senza conversione: " + file);
}

// Indici generali di un file letto, ricavati con lo schema e non file per file: codici degli elementi degli elenchi
// principali, e titolare degli elementi che appartengono a un altro (vedi Lettura).
void indicizza(const std::string& file, const std::string& schema, int versione, const JsonDati& d, Lettura& out) {
    for (const auto& [chiave, valore] : d.items()) {
        if (!valore.is_array()) continue;
        const std::string elenco = "/" + escape(chiave);
        const auto livelli = SchemiDati::incorporati().livelli(schema, versione, elenco + "/0");
        const JsonDati* titolare = livelli.size() == 3 ? livelli[2].titolare : nullptr;
        const std::string campo = titolare ? titolare->at("campo").get<std::string>() : std::string();
        auto& codici = out.codici[{file, chiave}];
        for (std::size_t i = 0; i < valore.size(); ++i) {
            const JsonDati& e = valore[i];
            if (!e.is_object()) continue;
            const auto c = e.find("codice");
            if (c != e.end() && c->is_string()) codici.emplace(c->get<std::string>(), i);
            const auto t = titolare ? e.find(campo) : e.end();
            if (t != e.end() && t->is_string()) out.titolari[file][indice(elenco, i)] = t->get<std::string>();
        }
    }
}

// Conta i valori di un documento (gia' conforme allo schema) secondo la loro classe: la marcatura piu' vicina, come nel
// filtro delle uscite. Un valore e' un testo, un numero o un vero/falso; gli oggetti e gli elenchi si attraversano.
void conta_classi(const JsonDati& radice, const JsonDati& valore, const JsonDati& nodo, Classe classe, Lettura::ValoriRiservati& out,
                  int profondita = 0) {
    if (profondita > 64) return;   // lo schema dei file e' molto meno profondo
    const JsonDati& definizione = nodo.is_object() && nodo.contains("$ref") ? SchemiDati::risolvi(radice, nodo) : nodo;
    for (const JsonDati* n : {&definizione, &nodo})
        if (n->is_object() && n->contains("x-kzero-classe")) {
            const std::string c = n->at("x-kzero-classe").get<std::string>();
            classe = c == "personale" ? Classe::Personale : c == "particolare" ? Classe::Particolare : Classe::Comune;
        }
    if (valore.is_object()) {
        const auto props = definizione.find("properties");
        for (const auto& [k, v] : valore.items()) {
            const bool previsto = props != definizione.end() && props->contains(k);
            conta_classi(radice, v, previsto ? props->at(k) : JsonDati::object(), classe, out, profondita + 1);
        }
        return;
    }
    if (valore.is_array()) {
        const auto items = definizione.find("items");
        for (const auto& v : valore)
            conta_classi(radice, v, items != definizione.end() && items->is_object() ? *items : JsonDati::object(), classe, out,
                         profondita + 1);
        return;
    }
    if (classe == Classe::Personale) ++out.personali;
    if (classe == Classe::Particolare) ++out.particolari;
}

Problem problema(Check c, const std::string& file, std::string puntatore, std::vector<Param> param = {}, int variante = 0) {
    return Problem{c, variante, Source{file, std::move(puntatore), 0, 0}, std::move(param)};
}

}  // namespace

void leggi_file(const std::string& file, const std::string& testo, Lettura& out, const ValoriCassaforte* cassaforte) {
    const FileDati* trovato = file_dati_per_nome(file);
    if (!trovato) throw std::logic_error("file che non fa parte del formato: " + file);
    const FileDati& f = *trovato;
    out.presenti.insert(f.file);
    if (const auto codifica = codifica_sbagliata(testo)) {
        Problem p = codifica->utf16 ? problema(Check::NotUtf8, f.file, "", {}, 1)
                                    : problema(Check::NotUtf8, f.file, "",
                                               {number_param("riga", codifica->riga), number_param("colonna", codifica->colonna)}, 0);
        p.where.line = codifica->riga;
        p.where.column = codifica->colonna;
        out.problemi.push_back(p);
        return;
    }
    // Un byte zero non puo' stare in un file JSON (nemmeno dentro un testo), e la libreria JSON lo prende per la fine del
    // file: un file valido seguito da zeri e da altro testo verrebbe letto a meta' (K-009, file rovinati o preparati).
    if (const std::size_t zero = testo.find('\0'); zero != std::string::npos) {
        const auto [riga, colonna] = riga_e_colonna(testo, zero);
        Problem p = problema(Check::InvalidJson, f.file, "", {number_param("riga", riga), number_param("colonna", colonna)}, 1);
        p.where.line = riga;
        p.where.column = colonna;
        out.problemi.push_back(p);
        return;
    }
    Analisi a = analizza(testo);
    if (!a.documento) {
        Problem p = problema(Check::InvalidJson, f.file, "", {number_param("riga", a.riga), number_param("colonna", a.colonna)},
                             a.numero_enorme ? 2 : 0);
        p.where.line = a.riga;
        p.where.column = a.colonna;
        out.problemi.push_back(p);
        return;
    }
    if (!a.chiavi_ripetute.empty()) {
        for (const std::string& p : a.chiavi_ripetute) out.problemi.push_back(problema(Check::DuplicateKey, f.file, p));
        return;
    }
    JsonDati d = std::move(*a.documento);
    if (!d.is_object() || !d.contains("schema") || !d.at("schema").is_string()) {
        out.problemi.push_back(problema(Check::WrongSchema, f.file, d.is_object() ? "/schema" : "",
                                        {text_param("atteso", f.schema)}, 0));
        return;
    }
    if (d.at("schema").get<std::string>() != f.schema) {
        out.problemi.push_back(problema(Check::WrongSchema, f.file, "/schema",
                                        {text_param("trovato", d.at("schema").get<std::string>()), text_param("atteso", f.schema)}, 1));
        return;
    }
    std::string supportate;
    for (int v : f.versioni) supportate += (supportate.empty() ? "" : ", ") + std::to_string(v);
    const auto versione = d.find("schema_versione");
    if (versione == d.end() || !versione->is_number_integer()) {
        out.problemi.push_back(problema(Check::UnsupportedVersion, f.file, versione == d.end() ? "" : "/schema_versione",
                                        {text_param("supportate", supportate)}, 1));
        return;
    }
    const long long v = versione->get<long long>();
    if (std::find(f.versioni.begin(), f.versioni.end(), v) == f.versioni.end()) {
        out.problemi.push_back(problema(Check::UnsupportedVersion, f.file, "/schema_versione",
                                        {number_param("versione", v), text_param("supportate", supportate)}, 0));
        return;
    }
    // Cassaforte (K-010): i valori in chiaro si contano prima dell'unione; poi i valori della cassaforte si mettono ognuno
    // nel suo elemento, per codice (classi.hpp). Un valore che non trova il suo posto ferma il file.
    const bool con_cassaforte = out.cassaforte_dichiarata && f.file != "pacchetto.json";
    if (con_cassaforte) {
        const ValoriPerClasse chiari = conta_valori(d, f.schema, static_cast<int>(v));
        out.in_chiaro.personali += chiari.personali;
        out.in_chiaro.particolari += chiari.particolari;
        if (cassaforte) {
            std::vector<const Voce*> voci;
            for (const Voce* x : cassaforte->voci)
                if (x->file == f.file) voci.push_back(x);
            if (!voci.empty() && !unisci(f.file, d, voci, out.problemi)) return;
        }
    }
    std::vector<ErroreSchema> errori = valida(d, f.schema, static_cast<int>(v));
    if (con_cassaforte && !(out.leggibili.personali && out.leggibili.particolari)) {
        // Per chi non puo' leggere una classe, i campi obbligatori di quella classe valgono come presenti ma non leggibili.
        // Un dato personale puo' stare tra i legati (luoghi sensibili), che si decifrano solo con tutti e due i permessi: senza
        // tutti e due non si sa quale manca davvero.
        errori.erase(std::remove_if(errori.begin(), errori.end(),
                                    [&](const ErroreSchema& e) {
                                        if (e.campo_mancante.empty()) return false;
                                        const Classe c = SchemiDati::incorporati().classe(
                                            f.schema, static_cast<int>(v), e.puntatore + "/" + escape(e.campo_mancante));
                                        return (c == Classe::Personale && !(out.leggibili.personali && out.leggibili.particolari)) ||
                                               (c == Classe::Particolare && !out.leggibili.particolari);
                                    }),
                     errori.end());
    }
    if (!errori.empty()) {
        for (const ErroreSchema& e : errori) {
            // Il valore sbagliato di un campo personale o particolare segue le classi dei dati.
            Param dettaglio = text_param("dettaglio", e.messaggio);
            const Classe classe = SchemiDati::incorporati().classe(f.schema, static_cast<int>(v), e.puntatore);
            if (classe != Classe::Comune) {
                dettaglio.sensitivity = classe == Classe::Personale ? Param::Sensitivity::Personal : Param::Sensitivity::Particular;
                dettaglio.hidden = e.messaggio_senza_valore;
            }
            // K-014: un campo obbligatorio particolare che manca rivela un dato particolare (variante 1): puo' essere obbligatorio
            // solo per alcuni valori di un altro dato particolare (es. la descrizione del tipo per un servizio "altro", che dice il
            // tipo di servizio). Come per un valore sbagliato, conta la classe del campo, ricavata dallo schema.
            const bool particolare_mancante =
                !e.campo_mancante.empty() && SchemiDati::incorporati().classe(f.schema, static_cast<int>(v),
                                                                              e.puntatore + "/" + escape(e.campo_mancante)) ==
                                                 Classe::Particolare;
            out.problemi.push_back(problema(Check::SchemaViolation, f.file, e.puntatore, {dettaglio}, particolare_mancante ? 1 : 0));
        }
        return;
    }
    converti(f.file, d, out.pacchetto);
    indicizza(f.file, f.schema, static_cast<int>(v), d, out);
    const JsonDati& schema = *SchemiDati::incorporati().schema(f.schema, static_cast<int>(v));
    Lettura::ValoriRiservati valori;
    conta_classi(schema, d, schema, Classe::Comune, valori);
    if (valori.personali + valori.particolari > 0) out.riservati[f.file] = valori;
    out.versioni[f.file] = static_cast<int>(v);
    out.documenti[f.file] = std::move(d);
}

Lettura leggi_pacchetto(const Sorgente& sorgente, const ValoriCassaforte* cassaforte) {
    Lettura out;
    std::set<std::string>& presenti = out.presenti;
    std::set<std::string> cartelle;   // cartelle con il nome di un file del formato
    for (const auto& v : sorgente.voci()) {
        if (file_dati_per_nome(v.nome)) {
            presenti.insert(v.nome);
            if (v.cartella) {
                // Un errore proprio: il file c'e' (non mancano i suoi elementi), ma non si puo' leggere. Cosi' i
                // controlli sul contenuto non partono e non si hanno errori a catena sui codici mancanti.
                cartelle.insert(v.nome);
                out.problemi.push_back(problema(Check::DirectoryInsteadOfFile, v.nome, "", {text_param("file", v.nome)}));
            }
            continue;
        }
        // La cassaforte e il suo registro (K-010) li legge chi apre la cassaforte, non questo lettore.
        if (v.nome == cassaforte::kFile && !v.cartella) {
            out.cassaforte_presente = true;
            continue;
        }
        if (v.nome == registro::kFile && !v.cartella) continue;
        out.problemi.push_back(problema(Check::UnknownFile, v.nome, "", {text_param("file", v.nome)}, v.cartella ? 1 : 0));
    }
    if (!presenti.count("pacchetto.json")) out.problemi.push_back(problema(Check::MissingManifest, "pacchetto.json", ""));

    const ValoriCassaforte* usabili = nullptr;   // i valori della cassaforte, se sono di questa chiusura
    for (const FileDati& f : file_dati()) {
        if (presenti.count(f.file) && !cartelle.count(f.file)) {
            FileLetto letto = sorgente.leggi(f.file, kMassimoByteFile);
            switch (letto.esito) {
                case FileLetto::Esito::Letto: leggi_file(f.file, letto.testo, out, usabili); break;
                case FileLetto::Esito::TroppoGrande:
                    out.problemi.push_back(
                        problema(Check::FileTooBig, f.file, "", {number_param("massimo", static_cast<long long>(kMassimoMBFile))}));
                    break;
                case FileLetto::Esito::Illeggibile: out.problemi.push_back(problema(Check::UnreadableFile, f.file, "")); break;
                case FileLetto::Esito::CollegamentoRotto:
                    out.problemi.push_back(problema(Check::UnreadableFile, f.file, "", {}, 1));
                    break;
                case FileLetto::Esito::NonNormale: out.problemi.push_back(problema(Check::NotRegularFile, f.file, "")); break;
                case FileLetto::Esito::FuoriCartella: out.problemi.push_back(problema(Check::LinkOutsideFolder, f.file, "")); break;
            }
        }
        if (f.file != "pacchetto.json") continue;
        // Dopo il manifesto si sa se il pacchetto ha una cassaforte (versione 2): da qui in poi si legge di conseguenza.
        if (!out.usato("pacchetto.json")) continue;
        const kzero::Manifest& m = out.pacchetto.manifest;
        out.cassaforte_dichiarata = !m.vault_id.empty();
        if (!out.cassaforte_dichiarata) {
            if (out.cassaforte_presente)
                out.problemi.push_back(problema(Check::VaultUndeclared, "pacchetto.json",
                                                out.versioni.at("pacchetto.json") >= 2 ? "/cassaforte" : ""));
            continue;
        }
        out.leggibili = Visibilita{false, false};
        if (!out.cassaforte_presente) {
            out.problemi.push_back(problema(Check::VaultMissing, "pacchetto.json", "/cassaforte"));
            continue;
        }
        if (!cassaforte) continue;
        if (cassaforte->id != m.vault_id) throw std::logic_error("lettura: valori di un'altra cassaforte");
        if (static_cast<long long>(cassaforte->generazione) != m.vault_generation) {
            out.problemi.push_back(problema(Check::VaultGenerationMismatch, "pacchetto.json", "/cassaforte/generazione",
                                            {number_param("manifesto", m.vault_generation),
                                             number_param("cassaforte", static_cast<long long>(cassaforte->generazione))}));
            continue;
        }
        usabili = cassaforte;
        out.leggibili = cassaforte->leggibili;
    }

    // Valori della cassaforte per file che non ci sono o che non sono stati letti: senza il loro elemento (uno per file e
    // classe, non uno per valore).
    if (usabili) {
        std::set<std::pair<std::string, int>> senza_file;
        for (const Voce* x : usabili->voci)
            if (!presenti.count(x->file) || cartelle.count(x->file)) senza_file.insert({x->file, x->classe == Classe::Particolare ? 1 : 0});
        for (const auto& [file, classe] : senza_file)
            out.problemi.push_back(problema(Check::VaultValueOrphan, file, "",
                                            {text_param("file", file), text_param("elemento", "il file " + file + ", che manca")}, classe));
    }
    // Dati personali o particolari in chiaro: nei file normali accanto alla cassaforte, o in un pacchetto senza cassaforte
    // che non dice di avere dati inventati.
    if (out.usato("pacchetto.json")) {
        const kzero::Manifest& m = out.pacchetto.manifest;
        Lettura::ValoriRiservati chiari = out.in_chiaro;
        if (!out.cassaforte_dichiarata) {
            chiari = {};
            for (const auto& [file, x] : out.riservati) {
                chiari.personali += x.personali;
                chiari.particolari += x.particolari;
            }
        }
        if (chiari.personali + chiari.particolari > 0 && (out.cassaforte_dichiarata || !m.invented))
            out.problemi.push_back(problema(Check::ClearPersonalData, "", "",
                                            {number_param("personali", static_cast<long long>(chiari.personali)),
                                             number_param("particolari", static_cast<long long>(chiari.particolari))},
                                            out.cassaforte_dichiarata ? 1 : 0));
    }
    return out;
}

std::optional<ManifestoLetto> leggi_manifesto(const Sorgente& sorgente) {
    const FileLetto letto = sorgente.leggi("pacchetto.json", kMassimoByteFile);
    if (letto.esito != FileLetto::Esito::Letto) return std::nullopt;
    Lettura l;
    leggi_file("pacchetto.json", letto.testo, l);
    if (!l.usato("pacchetto.json")) return std::nullopt;
    const kzero::Manifest& m = l.pacchetto.manifest;
    ManifestoLetto out;
    out.codice = m.code;
    out.versione = l.versioni.at("pacchetto.json");
    out.cassaforte = !m.vault_id.empty();
    out.id = m.vault_id;
    out.generazione = static_cast<std::uint32_t>(m.vault_generation);
    out.dati_inventati = m.invented;
    out.documento = l.documenti.at("pacchetto.json");
    return out;
}

}  // namespace kzero::api
