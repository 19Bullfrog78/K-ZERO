// K-Zero API - password della cassaforte (vedi password.hpp).
#include "password.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "unicode.hpp"

#if defined(__unix__) || defined(__APPLE__)
#define KZERO_TERMINALE_POSIX 1
#include <csignal>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace kzero::api::password {

// L'elenco incorporato quando si compila (cmake/elenco_password.cpp): password normalizzate (NFC e minuscole), in ordine,
// separate da '\n'.
extern const unsigned char kPasswordComuni[];
extern const std::size_t kPasswordComuniByte;
extern const std::size_t kPasswordComuniQuante;

using cripto::Segreto;

namespace {

const std::vector<std::string_view>& elenco_comuni() {
    static const std::vector<std::string_view> elenco = [] {
        std::vector<std::string_view> v;
        v.reserve(kPasswordComuniQuante);
        const std::string_view tutto(reinterpret_cast<const char*>(kPasswordComuni), kPasswordComuniByte);
        std::size_t i = 0;
        while (i < tutto.size()) {
            const std::size_t fine = tutto.find('\n', i);
            if (fine == std::string_view::npos) break;
            v.push_back(tutto.substr(i, fine - i));
            i = fine + 1;
        }
        return v;
    }();
    return elenco;
}

// Normalizza in una memoria protetta; con "minuscole" anche il case folding.
Segreto normalizza_in(std::string_view testo, bool minuscole, Problema& problema, std::size_t& caratteri) {
    problema = Problema::Nessuno;
    caratteri = 0;
    if (testo.empty()) {
        problema = Problema::Vuota;
        return {};
    }
    if (testo.size() > kMassimoByte) {
        problema = Problema::TroppoLunga;
        return {};
    }
    const std::size_t capienza = unicode::capienza_per(testo.size());
    Segreto lavoro(capienza * sizeof(std::int32_t));   // multiplo di 4 byte: allineato (sodium_malloc)
    const unicode::Risultato r =
        unicode::normalizza(testo, minuscole, reinterpret_cast<std::int32_t*>(lavoro.dati()), capienza);
    switch (r.esito) {
        case unicode::Esito::Normalizzato: break;
        case unicode::Esito::NonUtf8: problema = Problema::NonUtf8; return {};
        case unicode::Esito::NonAssegnato:
        case unicode::Esito::Controllo: problema = Problema::CarattereNonAmmesso; return {};
        case unicode::Esito::TroppoLungo: problema = Problema::TroppoLunga; return {};
    }
    if (r.caratteri > kMassimoCaratteri) {
        problema = Problema::TroppoLunga;
        return {};
    }
    caratteri = r.caratteri;
    lavoro.usa(r.byte);
    return lavoro;
}

// I caratteri (code point) di un testo UTF-8 valido, in memoria protetta.
Segreto caratteri_di(std::string_view t, std::size_t& n) {
    Segreto out((t.size() + 1) * sizeof(std::int32_t));
    auto* cp = reinterpret_cast<std::int32_t*>(out.dati());
    n = 0;
    for (std::size_t i = 0; i < t.size();) {
        const auto c = static_cast<unsigned char>(t[i]);
        const std::size_t l = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        std::int32_t v = l == 1 ? c : l == 2 ? (c & 0x1F) : l == 3 ? (c & 0x0F) : (c & 0x07);
        for (std::size_t k = 1; k < l && i + k < t.size(); ++k) v = (v << 6) | (static_cast<unsigned char>(t[i + k]) & 0x3F);
        cp[n++] = v;
        i += l;
    }
    return out;
}

// Un gruppo di al massimo 4 caratteri ripetuto dall'inizio alla fine (anche l'ultima ripetizione a meta').
bool ripetizione(const std::int32_t* c, std::size_t n) {
    for (std::size_t periodo = 1; periodo <= 4 && periodo < n; ++periodo) {
        bool ok = true;
        for (std::size_t i = periodo; i < n && ok; ++i) ok = c[i] == c[i - periodo];
        if (ok) return true;
    }
    return false;
}

// Una sequenza di caratteri consecutivi, in su o in giu' (le cifre ricominciano: ...8, 9, 0, 1...).
bool sequenza(const std::int32_t* c, std::size_t n) {
    const auto passo = [](std::int32_t a, std::int32_t b) -> int {
        if (a >= '0' && a <= '9' && b >= '0' && b <= '9') {
            if ((a + 1 - '0') % 10 == b - '0') return 1;
            if ((b + 1 - '0') % 10 == a - '0') return -1;
            return 0;
        }
        return b - a == 1 ? 1 : b - a == -1 ? -1 : 0;
    };
    if (n < 2) return false;
    const int verso = passo(c[0], c[1]);
    if (verso == 0) return false;
    for (std::size_t i = 2; i < n; ++i)
        if (passo(c[i - 1], c[i]) != verso) return false;
    return true;
}

bool separatore_o_cifra(char c) { return c == ' ' || c == '-' || c == '_' || c == '.' || c == '/' || (c >= '0' && c <= '9'); }

// Solo parole del contesto (unite, ripetute, separate da spazi, trattini, punti, barre o cifre)?
bool solo_contesto(std::string_view minuscola, const std::vector<std::string>& parole) {
    Segreto resto(minuscola.size() + 1);
    std::size_t n = 0;
    for (char c : minuscola)
        if (!separatore_o_cifra(c)) resto.dati()[n++] = static_cast<unsigned char>(c);
    if (n == 0) return false;   // solo cifre e separatori: lo dicono l'elenco, le ripetizioni e le sequenze
    const std::string_view r(reinterpret_cast<const char*>(resto.dati()), n);
    std::vector<bool> arrivo(n + 1, false);
    arrivo[0] = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (!arrivo[i]) continue;
        for (const std::string& p : parole)
            if (!p.empty() && r.compare(i, p.size(), p) == 0) arrivo[i + p.size()] = true;
    }
    return arrivo[n];
}

// Le parole del contesto nella stessa forma della password confrontata: minuscole, senza separatori ne' cifre.
std::vector<std::string> parole_del_contesto(const std::vector<std::string>& contesto) {
    std::vector<std::string> base = {"kzero", "k-zero", "cassaforte", "password", "parola d'ordine"};
    base.insert(base.end(), contesto.begin(), contesto.end());
    std::vector<std::string> out;
    for (const std::string& b : base) {
        Problema p = Problema::Nessuno;
        std::size_t c = 0;
        const Segreto m = normalizza_in(b, true, p, c);
        if (p != Problema::Nessuno) continue;
        std::string s;
        for (char x : m.testo())
            if (!separatore_o_cifra(x) && x != '\'') s += x;
        if (!s.empty()) out.push_back(s);
    }
    return out;
}

}  // namespace

std::string spiegazione(Problema p, std::size_t caratteri) {
    switch (p) {
        case Problema::Nessuno: return "password accettata";
        case Problema::Vuota: return "la password e' vuota";
        case Problema::TroppoLunga:
            return "la password e' troppo lunga: al massimo " + std::to_string(kMassimoCaratteri) + " caratteri";
        case Problema::NonUtf8: return "la password non e' testo UTF-8 valido";
        case Problema::CarattereNonAmmesso:
            return "la password contiene un carattere non ammesso (un carattere di controllo, es. una tabulazione, o un "
                   "carattere che lo standard Unicode non ha ancora assegnato)";
        case Problema::TroppoCorta:
            return "la password e' troppo corta: " + std::to_string(caratteri) + " caratteri, ne servono almeno " +
                   std::to_string(kMinimoCaratteri) + " (conviene una frase di qualche parola, facile da ricordare)";
        case Problema::Comune:
            return "la password e' nell'elenco delle password piu' usate (chi attacca le prova per prime): sceglierne un'altra";
        case Problema::Prevedibile:
            return "la password e' troppo prevedibile (fatta del nome del programma, della cassaforte, del codice "
                   "dell'utente o del pacchetto, di un gruppo di caratteri ripetuto o di una sequenza): sceglierne un'altra";
    }
    return "password non valida";
}

Segreto normalizza(const Segreto& scritta, Problema& problema) {
    std::size_t caratteri = 0;
    return normalizza_in(scritta.testo(), false, problema, caratteri);
}

Segreto nuova(const Segreto& scritta, const std::vector<std::string>& contesto, Problema& problema, std::size_t& caratteri) {
    Segreto norm = normalizza_in(scritta.testo(), false, problema, caratteri);
    if (problema != Problema::Nessuno) return {};
    if (caratteri < kMinimoCaratteri) {
        problema = Problema::TroppoCorta;
        return {};
    }
    std::size_t c2 = 0;
    const Segreto minuscola = normalizza_in(norm.testo(), true, problema, c2);
    if (problema != Problema::Nessuno) return {};
    const auto& elenco = elenco_comuni();
    if (std::binary_search(elenco.begin(), elenco.end(), minuscola.testo())) {
        problema = Problema::Comune;
        return {};
    }
    std::size_t n = 0;
    const Segreto cp = caratteri_di(minuscola.testo(), n);
    const auto* c = reinterpret_cast<const std::int32_t*>(cp.dati());
    if (ripetizione(c, n) || sequenza(c, n) || solo_contesto(minuscola.testo(), parole_del_contesto(contesto))) {
        problema = Problema::Prevedibile;
        return {};
    }
    return norm;
}

std::size_t password_comuni() { return elenco_comuni().size(); }

// --- Lettura ---------------------------------------------------------------------------------------------------------

namespace {

// Legge una riga da un descrittore, un byte alla volta (cosi' la riga dopo resta per la password seguente).
Segreto leggi_riga(int fd, bool& finito) {
    Segreto riga(kMassimoByte + 2);
    std::size_t n = 0;
    finito = false;
    for (;;) {
        unsigned char c = 0;
#if defined(KZERO_TERMINALE_POSIX)
        const ssize_t letti = ::read(fd, &c, 1);
        if (letti < 0 && errno == EINTR) continue;
#else
        (void)fd;
        const long letti = -1;
#endif
        if (letti <= 0) {
            finito = n == 0;
            break;
        }
        if (c == '\n') break;
        if (n <= kMassimoByte) riga.dati()[n++] = c;   // oltre il massimo si scarta, ma la riga e' troppo lunga
    }
    if (n > 0 && n <= kMassimoByte && riga.dati()[n - 1] == '\r') --n;
    riga.usa(n);
    return riga;
}

#if defined(KZERO_TERMINALE_POSIX)
// Se arriva un segnale (es. Ctrl-C) mentre l'eco e' spento, il terminale torna com'era prima di uscire.
int g_fd = -1;
struct termios g_salvato {};
void ripristina(int segnale) {
    if (g_fd >= 0) tcsetattr(g_fd, TCSANOW, &g_salvato);
    std::signal(segnale, SIG_DFL);
    std::raise(segnale);
}

class Terminale : public Ingresso {
public:
    explicit Terminale(int fd) : fd_(fd) {}
    ~Terminale() override { ::close(fd_); }
    bool terminale() const override { return true; }
    Segreto leggi(const std::string& domanda) override {
        (void)!::write(fd_, domanda.data(), domanda.size());
        struct termios prima {};
        const bool controllo = tcgetattr(fd_, &prima) == 0;
        const int segnali[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGTSTP};
        struct sigaction vecchie[5] {};
        if (controllo) {
            g_fd = fd_;
            g_salvato = prima;
            struct sigaction sa {};
            sa.sa_handler = ripristina;
            sigemptyset(&sa.sa_mask);
            for (int i = 0; i < 5; ++i) sigaction(segnali[i], &sa, &vecchie[i]);
            struct termios senza_eco = prima;
            senza_eco.c_lflag &= static_cast<tcflag_t>(~(ECHO | ECHOE | ECHOK | ECHONL));
            tcsetattr(fd_, TCSAFLUSH, &senza_eco);
        }
        bool finito = false;
        Segreto riga = leggi_riga(fd_, finito);
        if (controllo) {
            tcsetattr(fd_, TCSAFLUSH, &prima);
            for (int i = 0; i < 5; ++i) sigaction(segnali[i], &vecchie[i], nullptr);
            g_fd = -1;
        }
        (void)!::write(fd_, "\n", 1);
        if (finito) throw std::runtime_error("il terminale si e' chiuso prima della password");
        return riga;
    }

private:
    int fd_;
};
#endif

class IngressoStandard : public Ingresso {
public:
    bool terminale() const override { return false; }
    Segreto leggi(const std::string&) override {
        bool finito = false;
        Segreto riga = leggi_riga(0, finito);
        if (finito) throw std::runtime_error("l'ingresso standard e' finito prima della password");
        return riga;
    }
};

}  // namespace

std::unique_ptr<Ingresso> dal_terminale() {
#if defined(KZERO_TERMINALE_POSIX)
    const int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) return nullptr;
    if (!::isatty(fd)) {
        ::close(fd);
        return nullptr;
    }
    return std::make_unique<Terminale>(fd);
#else
    return nullptr;   // Windows: con il programma per Windows (DIARIO.md, limiti)
#endif
}

std::unique_ptr<Ingresso> dall_ingresso_standard() { return std::make_unique<IngressoStandard>(); }

}  // namespace kzero::api::password
