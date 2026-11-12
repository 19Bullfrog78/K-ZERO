// K-Zero API - lettura di un foglio di un file Excel (.xlsx): archivio in memoria (miniz) e XML a flusso (Expat). Vedi excel.hpp.
#include "excel.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include <expat.h>
#include <miniz.h>

#include "ricerca.hpp"

namespace kzero::api::excel {

const Limiti& limiti() {
    static const Limiti l;
    return l;
}

namespace {

using Motivo = ErroreExcel::Motivo;

[[noreturn]] void errore(Motivo m, const std::string& messaggio) { throw ErroreExcel(m, messaggio); }

// Il nome di una parte come si puo' mostrare in un messaggio: solo caratteri semplici, al massimo 100 (il nome viene dal file).
std::string nome_mostrabile(std::string_view nome) {
    std::string out;
    for (char c : nome.substr(0, 100)) out += (c >= 0x20 && c < 0x7f) ? c : '?';
    return out.size() < nome.size() ? out + "..." : out;
}

// Chiave di una parte: il formato non distingue maiuscole e minuscole nei nomi delle parti (Open Packaging Conventions).
std::string chiave_parte(std::string_view nome) {
    std::string k(nome);
    for (char& c : k)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return k;
}

// --- Archivio --------------------------------------------------------------------------------------------------------

class Archivio {
public:
    Archivio(std::string_view file, const Limiti& l) : l_(l) {
        static const unsigned char kXls[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
        if (file.size() >= sizeof kXls && std::memcmp(file.data(), kXls, sizeof kXls) == 0)
            errore(Motivo::VecchioXls, "Il file e' nel formato .xls di Excel 97-2003, che K-Zero non legge: aprirlo con Excel (o un "
                                       "programma simile) e salvarlo come \"Cartella di lavoro di Excel (.xlsx)\".");
        if (file.size() < 4 || file[0] != 'P' || file[1] != 'K')
            errore(Motivo::NonXlsx, "Il file non e' un file Excel .xlsx (non e' un archivio ZIP).");
        if (file.size() > l_.file_byte)
            errore(Motivo::TroppoGrande, "Il file e' piu' grande del massimo (" + std::to_string(l_.file_byte / (1024 * 1024)) + " MB).");
        // La lettura dalla memoria con una funzione nostra (API di miniz per le sorgenti dell'utente), non mz_zip_reader_init_mem:
        // quella calcola l'indirizzo dei byte anche oltre la fine del file, prima di leggerne zero (comportamento non definito,
        // trovato dal fuzzing del K-019). Qui si controlla la posizione prima di tutto.
        dati_ = file;
        z_.m_pRead = &Archivio::leggi_byte;
        z_.m_pIO_opaque = this;
        if (!mz_zip_reader_init(&z_, file.size(), 0)) {
            const mz_zip_error e = mz_zip_get_last_error(&z_);
            if (e == MZ_ZIP_UNSUPPORTED_MULTIDISK)
                errore(Motivo::PiuDischi, "Il file e' un archivio diviso su piu' dischi o piu' file: non si legge.");
            if (e == MZ_ZIP_UNSUPPORTED_ENCRYPTION) errore(Motivo::Cifrato, "Il file e' cifrato (protetto da una password): non si legge.");
            errore(Motivo::ArchivioRotto, "Il file .xlsx e' troncato o rovinato (l'archivio non si apre).");
        }
        zip_.aperto = true;
        const mz_uint n = mz_zip_reader_get_num_files(&z_);
        if (n > l_.parti)
            errore(Motivo::TroppeParti, "Il file ha piu' parti del massimo (" + std::to_string(l_.parti) + "): non e' un normale file Excel.");
        for (mz_uint i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z_, i, &st))
                errore(Motivo::ArchivioRotto, "Il file .xlsx e' rovinato (una voce dell'archivio non si legge).");
            if (st.m_is_encrypted) errore(Motivo::Cifrato, "Il file ha una parte cifrata (protetta da una password): non si legge.");
            if (st.m_method != 0 && st.m_method != MZ_DEFLATED)
                errore(Motivo::CompressioneNonAmmessa, "Il file usa un metodo di compressione che K-Zero non legge (parte " +
                                                           nome_mostrabile(st.m_filename) + ").");
            if (!st.m_is_supported) errore(Motivo::ArchivioRotto, "Il file ha una parte che non si puo' leggere (parte " +
                                                                     nome_mostrabile(st.m_filename) + ").");
            const std::string k = chiave_parte(st.m_filename);
            if (!indice_.emplace(k, Voce{i, st.m_comp_size}).second)
                errore(Motivo::NomiRipetuti, "Il file ha due parti con lo stesso nome (" + nome_mostrabile(st.m_filename) +
                                                 "): non e' un normale file Excel.");
            nomi_.push_back(st.m_filename);
        }
    }
    Archivio(const Archivio&) = delete;
    Archivio& operator=(const Archivio&) = delete;

    const std::vector<std::string>& nomi() const { return nomi_; }

    // Il contenuto decompresso di una parte, contando i byte veri; nullopt se la parte non c'e'.
    std::optional<std::string> leggi(const std::string& nome) {
        const auto it = indice_.find(chiave_parte(nome));
        if (it == indice_.end()) return std::nullopt;
        Raccolta r{std::string(), it->second.compressi, &l_, &totale_, Motivo::ArchivioRotto, std::string(), false};
        if (!mz_zip_reader_extract_to_callback(&z_, it->second.indice, &Archivio::scrivi, &r, 0)) {
            if (r.fermato) errore(r.motivo, r.messaggio + " (parte " + nome_mostrabile(nome) + ").");
            const mz_zip_error e = mz_zip_get_last_error(&z_);
            if (e == MZ_ZIP_UNSUPPORTED_ENCRYPTION) errore(Motivo::Cifrato, "Il file ha una parte cifrata: non si legge.");
            if (e == MZ_ZIP_UNSUPPORTED_METHOD)
                errore(Motivo::CompressioneNonAmmessa, "Il file usa un metodo di compressione che K-Zero non legge.");
            errore(Motivo::ArchivioRotto, "Il file .xlsx e' troncato o rovinato: la parte " + nome_mostrabile(nome) +
                                              " non si decomprime o non torna con le sue impronte.");
        }
        return std::move(r.dati);
    }

private:
    struct Voce {
        mz_uint indice;
        std::uint64_t compressi;
    };
    struct Raccolta {
        std::string dati;
        std::uint64_t compressi;
        const Limiti* l;
        std::uint64_t* totale;
        Motivo motivo;
        std::string messaggio;
        bool fermato;
    };

    // Riceve i byte veri decompressi, un pezzo alla volta: i limiti si controllano qui, mai sulle dimensioni dichiarate.
    static size_t scrivi(void* opaco, mz_uint64 posizione, const void* pezzo, size_t n) {
        Raccolta& r = *static_cast<Raccolta*>(opaco);
        const auto ferma = [&](Motivo m, std::string messaggio) {
            r.motivo = m;
            r.messaggio = std::move(messaggio);
            r.fermato = true;
            return size_t{0};
        };
        if (posizione != r.dati.size()) return ferma(Motivo::ArchivioRotto, "Il file .xlsx e' rovinato");
        const std::uint64_t nuovo = r.dati.size() + static_cast<std::uint64_t>(n);
        if (nuovo > r.l->decompresso_parte)
            return ferma(Motivo::TroppoGrande, "Una parte del file, decompressa, supera il massimo di " +
                                                   std::to_string(r.l->decompresso_parte / (1024 * 1024)) + " MB");
        if (*r.totale + n > r.l->decompresso_totale)
            return ferma(Motivo::TroppoGrande, "Le parti del file, decompresse, superano il massimo di " +
                                                   std::to_string(r.l->decompresso_totale / (1024 * 1024)) + " MB in tutto");
        if (nuovo > r.l->soglia_rapporto && nuovo > r.l->rapporto_compressione * std::max<std::uint64_t>(r.compressi, 1))
            return ferma(Motivo::Bomba, "Una parte del file si decomprime piu' di " + std::to_string(r.l->rapporto_compressione) +
                                            " volte: e' una \"bomba\" di compressione, non un normale file Excel");
        r.dati.append(static_cast<const char*>(pezzo), n);
        *r.totale += n;
        return n;
    }

    // I byte del file da "posizione": mai oltre la fine (e senza calcolare indirizzi fuori dal file).
    static size_t leggi_byte(void* opaco, mz_uint64 posizione, void* dove, size_t n) {
        const std::string_view d = static_cast<const Archivio*>(opaco)->dati_;
        if (posizione >= d.size()) return 0;
        const std::size_t p = static_cast<std::size_t>(posizione);
        const std::size_t quanti = std::min(n, d.size() - p);
        std::memcpy(dove, d.data() + p, quanti);
        return quanti;
    }

    // L'archivio aperto si chiude da solo, anche quando il costruttore si ferma dopo averlo aperto (es. troppe parti, una parte
    // cifrata): un membro si distrugge anche se il costruttore non finisce (perdita di memoria trovata dal fuzzing del K-019).
    struct Zip {
        Zip() { mz_zip_zero_struct(&z); }
        ~Zip() {
            if (aperto) mz_zip_reader_end(&z);
        }
        Zip(const Zip&) = delete;
        Zip& operator=(const Zip&) = delete;
        mz_zip_archive z;
        bool aperto = false;
    };
    Zip zip_;
    mz_zip_archive& z_ = zip_.z;
    std::string_view dati_;
    const Limiti& l_;
    std::map<std::string, Voce> indice_;
    std::vector<std::string> nomi_;
    std::uint64_t totale_ = 0;
};

// --- XML a flusso ----------------------------------------------------------------------------------------------------

struct Attributi {
    const char** a;
    // Il valore di un attributo per nome locale (senza il prefisso "x:"), oppure nullptr.
    const char* operator[](std::string_view nome) const {
        for (const char** p = a; p && *p; p += 2) {
            std::string_view n(*p);
            const std::size_t due = n.rfind(':');
            if (due != std::string_view::npos) n = n.substr(due + 1);
            if (n == nome) return p[1];
        }
        return nullptr;
    }
};

class Gestore {
public:
    virtual ~Gestore() = default;
    virtual void inizio(std::string_view nome, const Attributi& a) = 0;
    virtual void fine(std::string_view nome) = 0;
    virtual void testo(std::string_view t) = 0;
};

// Un errore trovato dentro un gestore: non si lancia attraverso il codice C di Expat, si ferma l'analisi e si rilancia dopo.
struct Fermata {
    Motivo motivo;
    std::string messaggio;
};

class Analisi {
public:
    Analisi(Gestore& g, const Limiti& l, std::string parte) : g_(g), l_(l), parte_(std::move(parte)) {
        p_ = XML_ParserCreate("UTF-8");
        if (!p_) throw std::bad_alloc();
        XML_SetUserData(p_, this);
        XML_SetElementHandler(p_, &Analisi::su_inizio, &Analisi::su_fine);
        XML_SetCharacterDataHandler(p_, &Analisi::su_testo);
        XML_SetStartDoctypeDeclHandler(p_, &Analisi::su_doctype);
    }
    ~Analisi() { XML_ParserFree(p_); }
    Analisi(const Analisi&) = delete;
    Analisi& operator=(const Analisi&) = delete;

    void analizza(std::string_view xml) {
        // A pezzi (la lunghezza per Expat e' un int); l'ultimo dice che il documento e' finito.
        constexpr std::size_t kPezzo = 1 << 20;
        std::size_t i = 0;
        do {
            const std::size_t n = std::min(kPezzo, xml.size() - i);
            const bool ultimo = i + n == xml.size();
            if (XML_Parse(p_, xml.data() + i, static_cast<int>(n), ultimo ? XML_TRUE : XML_FALSE) != XML_STATUS_OK) {
                if (fermata_) errore(fermata_->motivo, fermata_->messaggio);
                errore(Motivo::XmlMalformato, "Il file .xlsx e' rovinato: la parte " + nome_mostrabile(parte_) +
                                                  " non e' XML valido (riga " + std::to_string(XML_GetCurrentLineNumber(p_)) +
                                                  ", colonna " + std::to_string(XML_GetCurrentColumnNumber(p_)) + ").");
            }
            i += n;
        } while (i < xml.size());
        if (fermata_) errore(fermata_->motivo, fermata_->messaggio);
    }

private:
    void ferma(Motivo m, std::string messaggio) {
        if (!fermata_) fermata_ = Fermata{m, std::move(messaggio)};
        XML_StopParser(p_, XML_FALSE);
    }
    template <class F>
    void protetto(F f) {
        if (fermata_) return;
        try {
            f();
        } catch (const ErroreExcel& e) {
            ferma(e.motivo, e.what());
        } catch (const std::bad_alloc&) {
            ferma(Motivo::TroppoGrande, "Memoria esaurita leggendo il file.");
        }
    }
    static std::string_view locale(const XML_Char* nome) {
        std::string_view n(nome);
        const std::size_t due = n.rfind(':');
        return due == std::string_view::npos ? n : n.substr(due + 1);
    }
    static void XMLCALL su_inizio(void* u, const XML_Char* nome, const XML_Char** attr) {
        Analisi& a = *static_cast<Analisi*>(u);
        a.protetto([&] {
            if (++a.profondita_ > a.l_.profondita)
                errore(Motivo::XmlMalformato, "Il file .xlsx ha la parte " + nome_mostrabile(a.parte_) + " annidata oltre il massimo.");
            a.g_.inizio(locale(nome), Attributi{attr});
        });
    }
    static void XMLCALL su_fine(void* u, const XML_Char* nome) {
        Analisi& a = *static_cast<Analisi*>(u);
        a.protetto([&] {
            --a.profondita_;
            a.g_.fine(locale(nome));
        });
    }
    static void XMLCALL su_testo(void* u, const XML_Char* s, int n) {
        Analisi& a = *static_cast<Analisi*>(u);
        a.protetto([&] { a.g_.testo(std::string_view(s, static_cast<std::size_t>(n))); });
    }
    static void XMLCALL su_doctype(void* u, const XML_Char*, const XML_Char*, const XML_Char*, int) {
        Analisi& a = *static_cast<Analisi*>(u);
        a.ferma(Motivo::Doctype, "Il file .xlsx ha una dichiarazione DOCTYPE (DTD) nella parte " + nome_mostrabile(a.parte_) +
                                     ": un normale file Excel non ne ha, e K-Zero non la legge.");
    }

    Gestore& g_;
    const Limiti& l_;
    std::string parte_;
    XML_Parser p_ = nullptr;
    int profondita_ = 0;
    std::optional<Fermata> fermata_;
};

void analizza(std::string_view xml, Gestore& g, const Limiti& l, const std::string& parte) {
    Analisi a(g, l, parte);
    a.analizza(xml);
}

// Raccoglie un testo fino a un massimo (i byte grezzi, prima della decodifica degli "_xHHHH_", che possono essere 7 per lettera).
class Testo {
public:
    explicit Testo(std::size_t massimo) : massimo_(massimo) {}
    void aggiungi(std::string_view t) {
        if (troppo_) return;
        if (grezzo_.size() + t.size() > massimo_ * 7 + 7) {
            troppo_ = true;
            grezzo_.clear();
            return;
        }
        grezzo_.append(t);
    }
    void svuota() {
        grezzo_.clear();
        troppo_ = false;
    }
    bool vuoto() const { return grezzo_.empty() && !troppo_; }
    // Il testo decodificato; nullopt se supera il massimo.
    std::optional<std::string> finale() const {
        if (troppo_) return std::nullopt;
        std::string d = decodifica_escape(grezzo_);
        if (d.size() > massimo_) return std::nullopt;
        return d;
    }

private:
    std::size_t massimo_;
    std::string grezzo_;
    bool troppo_ = false;
};

// --- Relazioni, cartella di lavoro --------------------------------------------------------------------------------------

struct Relazione {
    std::string id, tipo, destinazione;
    bool esterna = false;
};

class GestoreRelazioni : public Gestore {
public:
    void inizio(std::string_view nome, const Attributi& a) override {
        if (nome != "Relationship") return;
        Relazione r;
        if (const char* v = a["Id"]) r.id = v;
        if (const char* v = a["Type"]) r.tipo = v;
        if (const char* v = a["Target"]) r.destinazione = v;
        if (const char* v = a["TargetMode"]) r.esterna = std::string_view(v) == "External";
        relazioni.push_back(std::move(r));
    }
    void fine(std::string_view) override {}
    void testo(std::string_view) override {}
    std::vector<Relazione> relazioni;
};

struct FoglioDichiarato {
    std::string nome, relazione;
};

class GestoreCartella : public Gestore {
public:
    void inizio(std::string_view nome, const Attributi& a) override {
        if (nome != "sheet") return;
        FoglioDichiarato f;
        if (const char* v = a["name"]) f.nome = v;
        if (const char* v = a["id"]) f.relazione = v;   // r:id
        fogli.push_back(std::move(f));
    }
    void fine(std::string_view) override {}
    void testo(std::string_view) override {}
    std::vector<FoglioDichiarato> fogli;
};

bool finisce_con(std::string_view s, std::string_view fine) {
    return s.size() >= fine.size() && s.substr(s.size() - fine.size()) == fine;
}

// La parte a cui porta una relazione, dalla cartella della parte che la dichiara; nullopt se esce dall'archivio.
std::optional<std::string> risolvi(const std::string& cartella, const std::string& destinazione) {
    std::string percorso = !destinazione.empty() && destinazione[0] == '/' ? destinazione.substr(1) : cartella + destinazione;
    std::vector<std::string> pezzi;
    std::size_t i = 0;
    while (i <= percorso.size()) {
        const std::size_t j = std::min(percorso.find('/', i), percorso.size());
        const std::string p = percorso.substr(i, j - i);
        if (p == "..") {
            if (pezzi.empty()) return std::nullopt;
            pezzi.pop_back();
        } else if (!p.empty() && p != ".") {
            pezzi.push_back(p);
        }
        i = j + 1;
    }
    std::string out;
    for (const std::string& p : pezzi) out += (out.empty() ? "" : "/") + p;
    if (out.empty()) return std::nullopt;
    return out;
}

std::string cartella_di(const std::string& parte) {
    const std::size_t b = parte.rfind('/');
    return b == std::string::npos ? std::string() : parte.substr(0, b + 1);
}

std::string relazioni_di(const std::string& parte) {
    const std::size_t b = parte.rfind('/');
    const std::string cartella = b == std::string::npos ? std::string() : parte.substr(0, b + 1);
    const std::string nome = b == std::string::npos ? parte : parte.substr(b + 1);
    return cartella + "_rels/" + nome + ".rels";
}

std::vector<Relazione> relazioni(Archivio& a, const std::string& parte, const Limiti& l) {
    const std::optional<std::string> xml = a.leggi(relazioni_di(parte));
    if (!xml) return {};
    GestoreRelazioni g;
    analizza(*xml, g, l, relazioni_di(parte));
    return std::move(g.relazioni);
}

// --- Testi condivisi -------------------------------------------------------------------------------------------------

struct Condivisi {
    std::vector<std::string> testi;
    std::vector<bool> troppo_lunghi;
};

class GestoreCondivisi : public Gestore {
public:
    explicit GestoreCondivisi(const Limiti& l) : l_(l), t_(l.cella) {}
    void inizio(std::string_view nome, const Attributi&) override {
        if (nome == "si") {
            in_si_ = true;
            t_.svuota();
            if (out.testi.size() >= l_.stringhe_condivise)
                errore(Motivo::TroppiTesti, "Il file ha piu' testi del massimo (" + std::to_string(l_.stringhe_condivise) + ").");
        } else if (nome == "rPh") {
            ++fonetica_;
        } else if (nome == "t" && in_si_ && fonetica_ == 0) {
            in_t_ = true;
        }
    }
    void fine(std::string_view nome) override {
        if (nome == "si" && in_si_) {
            const std::optional<std::string> f = t_.finale();
            out.testi.push_back(f.value_or(std::string()));
            out.troppo_lunghi.push_back(!f);
            in_si_ = false;
        } else if (nome == "rPh") {
            if (fonetica_ > 0) --fonetica_;
        } else if (nome == "t") {
            in_t_ = false;
        }
    }
    void testo(std::string_view t) override {
        if (in_t_) t_.aggiungi(t);
    }
    Condivisi out;

private:
    const Limiti& l_;
    Testo t_;
    bool in_si_ = false;
    bool in_t_ = false;
    int fonetica_ = 0;
};

Condivisi condivisi_da_xml(std::string_view xml, const Limiti& l) {
    GestoreCondivisi g(l);
    analizza(xml, g, l, "testi condivisi");
    return std::move(g.out);
}

// --- Foglio ------------------------------------------------------------------------------------------------------------

// "AB12" -> colonna 28 e riga 12. nullopt se la forma non e' quella di un riferimento.
std::optional<std::pair<std::uint32_t, std::uint32_t>> riferimento(std::string_view r) {
    std::uint32_t colonna = 0;
    std::size_t i = 0;
    while (i < r.size() && ((r[i] >= 'A' && r[i] <= 'Z') || (r[i] >= 'a' && r[i] <= 'z'))) {
        if (i >= 3) return std::nullopt;
        const char c = static_cast<char>(r[i] >= 'a' ? r[i] - 'a' + 'A' : r[i]);
        colonna = colonna * 26 + static_cast<std::uint32_t>(c - 'A' + 1);
        ++i;
    }
    if (i == 0 || i == r.size() || colonna > 16384) return std::nullopt;
    std::uint32_t riga = 0;
    const auto [p, ec] = std::from_chars(r.data() + i, r.data() + r.size(), riga);
    if (ec != std::errc() || p != r.data() + r.size() || riga == 0 || riga > 1048576) return std::nullopt;
    return std::make_pair(colonna, riga);
}

std::optional<std::uint32_t> numero_riga(std::string_view r) {
    std::uint32_t n = 0;
    const auto [p, ec] = std::from_chars(r.data(), r.data() + r.size(), n);
    if (ec != std::errc() || p != r.data() + r.size() || n == 0 || n > 1048576) return std::nullopt;
    return n;
}

class GestoreFoglio : public Gestore {
public:
    GestoreFoglio(const Condivisi& c, const Limiti& l) : c_(c), l_(l), v_(l.cella), is_(l.cella) {}

    void inizio(std::string_view nome, const Attributi& a) override {
        if (nome == "row") {
            in_riga_ = true;
            const char* r = a["r"];
            std::optional<std::uint32_t> n = r ? numero_riga(r) : std::optional<std::uint32_t>(riga_ + 1);
            if (!n) errore(Motivo::CellaRotta, "Il foglio ha un numero di riga scritto male.");
            riga_ = *n;
            colonna_ = 0;
            corrente_ = Riga{riga_, {}};
        } else if (nome == "c" && in_riga_) {
            in_cella_ = true;
            const char* r = a["r"];
            if (r) {
                const auto rc = riferimento(r);
                if (!rc || rc->second != riga_)
                    errore(Motivo::CellaRotta, "Il foglio ha un riferimento di cella scritto male (riga " + std::to_string(riga_) + ").");
                colonna_ = rc->first;
            } else {
                ++colonna_;
            }
            const char* t = a["t"];
            tipo_ = t ? t : "n";
            v_.svuota();
            is_.svuota();
            in_v_ = in_is_t_ = false;
            ha_v_ = ha_is_ = false;
            fonetica_ = 0;
        } else if (in_cella_ && nome == "v") {
            in_v_ = true;
            ha_v_ = true;
        } else if (in_cella_ && nome == "is") {
            ha_is_ = true;
        } else if (in_cella_ && nome == "rPh") {
            ++fonetica_;
        } else if (in_cella_ && ha_is_ && nome == "t" && fonetica_ == 0) {
            in_is_t_ = true;
        }
    }

    void fine(std::string_view nome) override {
        if (nome == "c" && in_cella_) {
            chiudi_cella();
            in_cella_ = false;
        } else if (nome == "v") {
            in_v_ = false;
        } else if (nome == "t") {
            in_is_t_ = false;
        } else if (nome == "rPh") {
            if (fonetica_ > 0) --fonetica_;
        } else if (nome == "row" && in_riga_) {
            in_riga_ = false;
            if (!corrente_.celle.empty()) {
                std::sort(corrente_.celle.begin(), corrente_.celle.end(),
                          [](const Cella& x, const Cella& y) { return x.colonna < y.colonna; });
                for (std::size_t i = 1; i < corrente_.celle.size(); ++i)
                    if (corrente_.celle[i].colonna == corrente_.celle[i - 1].colonna)
                        errore(Motivo::CellaRotta, "Il foglio ha la stessa cella due volte (riga " + std::to_string(riga_) + ").");
                out.righe.push_back(std::move(corrente_));
            }
        }
    }

    void testo(std::string_view t) override {
        if (in_v_) v_.aggiungi(t);
        if (in_is_t_) is_.aggiungi(t);
    }

    Foglio out;

private:
    void chiudi_cella() {
        Cella c;
        c.colonna = colonna_;
        std::optional<std::string> valore;
        if (tipo_ == "s") {
            if (!ha_v_) return;
            const std::optional<std::string> v = v_.finale();
            std::size_t indice = 0;
            bool buono = v.has_value() && !v->empty();
            if (buono) {
                const auto [p, ec] = std::from_chars(v->data(), v->data() + v->size(), indice);
                buono = ec == std::errc() && p == v->data() + v->size() && indice < c_.testi.size();
            }
            if (!buono) errore(Motivo::CellaRotta, "Il foglio rimanda a un testo che non c'e' (riga " + std::to_string(riga_) + ").");
            if (c_.troppo_lunghi[indice]) {
                c.tipo = Cella::Tipo::TroppoLunga;
            } else {
                c.tipo = Cella::Tipo::Testo;
                c.testo = c_.testi[indice];
            }
        } else if (tipo_ == "inlineStr") {
            if (!ha_is_) return;
            valore = is_.finale();
            c.tipo = valore ? Cella::Tipo::Testo : Cella::Tipo::TroppoLunga;
        } else if (tipo_ == "e") {
            if (!ha_v_) return;
            c.tipo = Cella::Tipo::Errore;
        } else {
            // n (numero, anche senza t), str (testo di una formula: il valore salvato), b (vero o falso), d (data ISO).
            if (!ha_v_) return;   // senza valore (es. una formula mai calcolata): cella vuota
            valore = v_.finale();
            c.tipo = !valore           ? Cella::Tipo::TroppoLunga
                     : tipo_ == "b"   ? Cella::Tipo::VeroFalso
                     : tipo_ == "str" || tipo_ == "d" ? Cella::Tipo::Testo
                                                      : Cella::Tipo::Numero;
        }
        if (valore) c.testo = std::move(*valore);
        if (c.tipo == Cella::Tipo::Testo && c.testo.empty()) return;   // un testo vuoto e' una cella vuota
        if (riga_ > l_.righe)
            errore(Motivo::TroppeRighe, "Il foglio ha piu' righe del massimo (" + std::to_string(l_.righe) + ").");
        if (colonna_ > l_.colonne)
            errore(Motivo::TroppeColonne, "Il foglio ha valori oltre la colonna numero " + std::to_string(l_.colonne) + ", il massimo.");
        corrente_.celle.push_back(std::move(c));
    }

    const Condivisi& c_;
    const Limiti& l_;
    Testo v_, is_;
    bool in_riga_ = false, in_cella_ = false, in_v_ = false, in_is_t_ = false, ha_v_ = false, ha_is_ = false;
    int fonetica_ = 0;
    std::uint32_t riga_ = 0, colonna_ = 0;
    std::string tipo_;
    Riga corrente_;
};

Foglio foglio_da(std::string_view xml, const Condivisi& c, const Limiti& l) {
    GestoreFoglio g(c, l);
    analizza(xml, g, l, "foglio");
    Foglio f = std::move(g.out);
    std::sort(f.righe.begin(), f.righe.end(), [](const Riga& a, const Riga& b) { return a.numero < b.numero; });
    for (std::size_t i = 1; i < f.righe.size(); ++i)
        if (f.righe[i].numero == f.righe[i - 1].numero)
            errore(Motivo::CellaRotta, "Il foglio ha la stessa riga due volte (riga " + std::to_string(f.righe[i].numero) + ").");
    return f;
}

}  // namespace

std::string decodifica_escape(std::string_view t) {
    const auto esadecimale = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    const auto codice_a = [&](std::size_t i) -> int {   // "_xHHHH_" in i, oppure -1
        if (i + 7 > t.size() || t[i] != '_' || t[i + 1] != 'x' || t[i + 6] != '_') return -1;
        int v = 0;
        for (std::size_t k = i + 2; k < i + 6; ++k) {
            const int h = esadecimale(t[k]);
            if (h < 0) return -1;
            v = v * 16 + h;
        }
        return v;
    };
    const auto utf8 = [](std::string& out, std::uint32_t c) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    };
    std::string out;
    out.reserve(t.size());
    std::size_t i = 0;
    while (i < t.size()) {
        const int c = codice_a(i);
        if (c < 0) {
            out += t[i++];
            continue;
        }
        i += 7;
        if (c >= 0xD800 && c <= 0xDBFF) {   // prima meta' di una coppia: serve la seconda subito dopo
            const int d = codice_a(i);
            if (d >= 0xDC00 && d <= 0xDFFF) {
                i += 7;
                utf8(out, 0x10000u + ((static_cast<std::uint32_t>(c) - 0xD800u) << 10) + (static_cast<std::uint32_t>(d) - 0xDC00u));
            } else {
                utf8(out, 0xFFFD);
            }
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            utf8(out, 0xFFFD);
        } else {
            utf8(out, static_cast<std::uint32_t>(c));
        }
    }
    return out;
}

std::vector<std::string> nomi_delle_parti(std::string_view file, const Limiti& l) {
    Archivio a(file, l);
    return a.nomi();
}

std::vector<std::string> parti_dell_archivio(std::string_view file, const std::vector<std::string>& nomi, bool mancanti_ammesse,
                                             const Limiti& l) {
    Archivio a(file, l);
    std::vector<std::string> out;
    for (const std::string& n : nomi) {
        std::optional<std::string> p = a.leggi(n);
        if (!p && !mancanti_ammesse) errore(Motivo::ParteMancante, "Il file non ha la parte " + nome_mostrabile(n) + ".");
        out.push_back(p.value_or(std::string()));
    }
    return out;
}

std::vector<std::string> testi_condivisi_da_xml(std::string_view xml, const Limiti& l) { return condivisi_da_xml(xml, l).testi; }

Foglio foglio_da_xml(std::string_view xml, const std::vector<std::string>& condivisi, const Limiti& l) {
    Condivisi c{condivisi, std::vector<bool>(condivisi.size(), false)};
    return foglio_da(xml, c, l);
}

Foglio leggi_foglio(std::string_view file, const std::string& nome_foglio, const Limiti& l) {
    Archivio a(file, l);
    // La cartella di lavoro: dalla relazione "officeDocument" della radice.
    const char* kNonExcel = "Il file e' un archivio ZIP ma non un file Excel .xlsx (manca la cartella di lavoro).";
    std::optional<std::string> cartella;
    for (const Relazione& r : relazioni(a, "", l))
        if (!r.esterna && finisce_con(r.tipo, "/officeDocument")) cartella = risolvi("", r.destinazione);
    if (!cartella) errore(Motivo::ParteMancante, kNonExcel);
    const std::optional<std::string> xml_cartella = a.leggi(*cartella);
    if (!xml_cartella) errore(Motivo::ParteMancante, kNonExcel);
    GestoreCartella gc;
    analizza(*xml_cartella, gc, l, *cartella);
    const std::vector<Relazione> rel = relazioni(a, *cartella, l);
    const auto destinazione = [&](const Relazione& r) { return risolvi(cartella_di(*cartella), r.destinazione); };

    // Il foglio: quello col nome del profilo (confrontato come i nomi dei luoghi: maiuscole, accenti, spazi), o il primo.
    if (gc.fogli.empty()) errore(Motivo::FoglioMancante, "Il file non ha fogli.");
    const FoglioDichiarato* scelto = nullptr;
    if (nome_foglio.empty()) {
        scelto = &gc.fogli.front();
    } else {
        for (const FoglioDichiarato& f : gc.fogli)
            if (kzero::normalize_name(f.nome) == kzero::normalize_name(nome_foglio)) {
                if (scelto)
                    errore(Motivo::FoglioMancante, "Il file ha piu' fogli con il nome indicato dal profilo: non si sa quale leggere.");
                scelto = &f;
            }
        if (!scelto)
            errore(Motivo::FoglioMancante, "Il file non ha il foglio indicato dal profilo (ha " + std::to_string(gc.fogli.size()) +
                                               (gc.fogli.size() == 1 ? " foglio" : " fogli") + ").");
    }
    std::optional<std::string> parte_foglio, parte_condivisi;
    for (const Relazione& r : rel) {
        if (r.esterna) continue;
        if (r.id == scelto->relazione && !scelto->relazione.empty()) parte_foglio = destinazione(r);
        if (finisce_con(r.tipo, "/sharedStrings")) parte_condivisi = destinazione(r);
    }
    if (!parte_foglio) errore(Motivo::ParteMancante, "Il file non ha la parte del foglio da leggere.");
    Condivisi condivisi;
    if (parte_condivisi) {
        const std::optional<std::string> xml = a.leggi(*parte_condivisi);
        if (!xml) errore(Motivo::ParteMancante, "Il file non ha la parte dei testi condivisi che dichiara.");
        condivisi = condivisi_da_xml(*xml, l);
    }
    const std::optional<std::string> xml_foglio = a.leggi(*parte_foglio);
    if (!xml_foglio) errore(Motivo::ParteMancante, "Il file non ha la parte del foglio da leggere.");
    return foglio_da(*xml_foglio, condivisi, l);
}

}  // namespace kzero::api::excel
