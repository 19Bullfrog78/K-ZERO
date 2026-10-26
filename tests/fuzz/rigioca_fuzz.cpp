// K-Zero - ripete nelle prove normali i casi del fuzzing: ogni guasto trovato diventa un file in tests/fuzz/casi/<bersaglio>
// e questa prova lo rigioca a ogni compilazione (anche con i sanitizer). Con --semi prova ogni file del pacchetto demo
// nel suo bersaglio: il punto di partenza del fuzzing.
// Con --scrivi-semi <cartella> scrive i semi dei bersagli della cassaforte (K-010), che hanno chiavi casuali.
// Uso: rigioca_fuzz <bersaglio> <file o cartella>...   |   rigioca_fuzz --semi   |   rigioca_fuzz --scrivi-semi <cartella>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bersagli.hpp"
#include "lettore.hpp"

namespace {

std::vector<unsigned char> leggi(const std::filesystem::path& f) {
    std::ifstream in(f, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    int provati = 0;
    if (argc == 2 && std::string(argv[1]) == "--semi") {
        const kzero::api::PacchettoIncorporato demo("esempi/demo");
        for (const std::string& f : kzero::fuzz::file_del_formato()) {
            const kzero::api::FileLetto l = demo.leggi(f, kzero::api::kMassimoByteFile);
            const std::string testo = l.esito == kzero::api::FileLetto::Esito::Letto ? l.testo : std::string("{}");
            kzero::fuzz::prova_file_del_pacchetto(f, reinterpret_cast<const unsigned char*>(testo.data()), testo.size());
            ++provati;
        }
        provati += kzero::fuzz::prova_semi_cassaforte();
        std::printf("%d semi provati nei loro bersagli (file del demo e cassaforte)\n", provati);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--scrivi-semi") {
        kzero::fuzz::scrivi_semi_cassaforte(argv[2]);
        return 0;
    }
    if (argc < 3) {
        std::fprintf(stderr, "Uso: rigioca_fuzz <bersaglio> <file o cartella>...  oppure  rigioca_fuzz --semi\n");
        return 2;
    }
    const std::string bersaglio = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::vector<fs::path> casi;
        if (fs::is_directory(argv[i])) {
            for (const auto& e : fs::directory_iterator(argv[i]))
                if (e.is_regular_file()) casi.push_back(e.path());
        } else {
            casi.push_back(argv[i]);
        }
        for (const fs::path& c : casi) {
            const std::vector<unsigned char> dati = leggi(c);
            if (!kzero::fuzz::prova(bersaglio, dati.data(), dati.size())) {
                std::fprintf(stderr, "bersaglio sconosciuto: %s\n", bersaglio.c_str());
                return 2;
            }
            ++provati;
        }
    }
    std::printf("%d casi rigiocati senza guasti (%s)\n", provati, bersaglio.c_str());
    return 0;
}
