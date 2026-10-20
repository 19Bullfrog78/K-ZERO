// K-Zero API - scritture atomiche e blocco della cartella (vedi disco.hpp).
#include "disco.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "cripto.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#error "Scritture della cassaforte: per ora solo POSIX (Linux, macOS); Windows con il programma per Windows."
#endif

namespace kzero::api::disco {

namespace {

Passo passo_prove = nullptr;

void passo(const char* nome_passo, const std::string& nome) {
    if (passo_prove) passo_prove(nome_passo, nome);
}

std::string spiega(const std::string& cosa, int e) { return cosa + " (" + std::strerror(e) + ")"; }

// Porta sul disco la cartella (il rename e l'unlink diventano permanenti).
void sincronizza_cartella(const std::filesystem::path& cartella) {
    const int fd = ::open(cartella.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) throw ErroreDisco(spiega("Non si riesce ad aprire la cartella " + cartella.string(), errno));
    const int r = ::fsync(fd);
    const int e = errno;
    ::close(fd);
    // Alcuni file system non permettono fsync su una cartella (EINVAL): la sostituzione e' comunque atomica.
    if (r != 0 && e != EINVAL && e != EBADF) throw ErroreDisco(spiega("La cartella " + cartella.string() + " non va sul disco", e));
}

void scrivi_tutto(int fd, std::string_view contenuto, const std::string& nome) {
    std::size_t scritti = 0;
    while (scritti < contenuto.size()) {
        const ssize_t n = ::write(fd, contenuto.data() + scritti, contenuto.size() - scritti);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw ErroreDisco(spiega("Scrittura di " + nome + " non riuscita", errno));
        }
        scritti += static_cast<std::size_t>(n);
    }
}

}  // namespace

void per_le_prove_a_ogni_passo(Passo p) { passo_prove = p; }

void scrivi_atomico(const std::filesystem::path& cartella, const std::string& nome, std::string_view contenuto) {
    const std::string temporaneo = "." + nome + ".kzero-" + cripto::esadecimale(cripto::casuali(8));
    const std::filesystem::path percorso_temporaneo = cartella / temporaneo;
    const int fd = ::open(percorso_temporaneo.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0666);
    if (fd < 0) throw ErroreDisco(spiega("Non si riesce a creare un file nella cartella " + cartella.string(), errno));
    try {
        passo("temporaneo creato", nome);
        scrivi_tutto(fd, contenuto, nome);
        if (::fsync(fd) != 0) throw ErroreDisco(spiega("Il file " + nome + " non va sul disco", errno));
        if (::close(fd) != 0) throw ErroreDisco(spiega("Chiusura di " + nome + " non riuscita", errno));
    } catch (...) {
        ::close(fd);
        ::unlink(percorso_temporaneo.c_str());
        throw;
    }
    try {
        passo("temporaneo scritto", nome);
        if (::rename(percorso_temporaneo.c_str(), (cartella / nome).c_str()) != 0)
            throw ErroreDisco(spiega("Il file " + nome + " non si riesce a sostituire", errno));
    } catch (...) {
        ::unlink(percorso_temporaneo.c_str());
        throw;
    }
    passo("sostituito", nome);
    sincronizza_cartella(cartella);
}

void togli(const std::filesystem::path& cartella, const std::string& nome) {
    if (::unlink((cartella / nome).c_str()) != 0 && errno != ENOENT)
        throw ErroreDisco(spiega("Il file " + nome + " non si riesce a togliere", errno));
    sincronizza_cartella(cartella);
}

void azzera_inizio(const std::filesystem::path& cartella, const std::string& nome, std::size_t byte) {
    const int fd = ::open((cartella / nome).c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw ErroreDisco(spiega("Il file " + nome + " non si riesce ad aprire", errno));
    try {
        struct stat st {};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) throw ErroreDisco("Il file " + nome + " non e' un file normale");
        const std::size_t n = std::min<std::size_t>(byte, static_cast<std::size_t>(st.st_size));
        const std::string zeri(n, '\0');
        scrivi_tutto(fd, zeri, nome);
        if (::fsync(fd) != 0) throw ErroreDisco(spiega("Il file " + nome + " non va sul disco", errno));
    } catch (...) {
        ::close(fd);
        throw;
    }
    ::close(fd);
}

Blocco::Blocco(const std::filesystem::path& cartella) {
    fd_ = ::open(cartella.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd_ < 0) throw ErroreDisco(spiega("Non si riesce ad aprire la cartella " + cartella.string() + " per bloccarla", errno));
    for (;;) {
        if (::flock(fd_, LOCK_EX) == 0) return;
        if (errno == EINTR) continue;
        const int e = errno;
        ::close(fd_);
        fd_ = -1;
        throw ErroreDisco(spiega("La cartella " + cartella.string() + " non si riesce a bloccare", e));
    }
}

Blocco::~Blocco() {
    if (fd_ >= 0) {
        ::flock(fd_, LOCK_UN);
        ::close(fd_);
    }
}

}  // namespace kzero::api::disco
