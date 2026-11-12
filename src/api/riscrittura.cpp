// K-Zero API - riscrivere i dati di un pacchetto chiuso (vedi riscrittura.hpp).
#include "riscrittura.hpp"

#include <optional>

#include "accesso.hpp"
#include "disco.hpp"
#include "errori.hpp"
#include "formato_dati.hpp"

namespace kzero::api::riscrittura {

namespace fs = std::filesystem;

void scrivi_o_errore(const fs::path& cartella, const std::string& nome, const std::string& contenuto) {
    try {
        disco::scrivi_atomico(cartella, nome, contenuto);
    } catch (const disco::ErroreDisco& e) {
        throw ErroreKZero(Errore::ScritturaNonRiuscita, std::string("Scrittura non riuscita: ") + e.what() + ".");
    }
}

// I file di un pacchetto chiuso, in memoria: file normali, manifesto con la cassaforte, cassaforte e registro.
PacchettoInMemoria chiuso_in_memoria(const chiusura::Separazione& s, const std::string& id, std::uint32_t generazione,
                                     const std::string& cassaforte_file, const std::string& registro_file) {
    PacchettoInMemoria m;
    for (const auto& [file, doc] : s.normali)
        m.file[file] = chiusura::testo_file(file == "pacchetto.json" ? chiusura::manifesto_chiuso(doc, id, generazione) : doc);
    m.file[cassaforte::kFile] = cassaforte_file;
    m.file[registro::kFile] = registro_file;
    return m;
}

// Riapre il pacchetto chiuso (con la password, come un'apertura vera) e controlla che i dati uniti siano quelli di
// "originale". Errore K-ERR-035 se no.
// con_registro: controlla anche il registro (nella richiusura la voce nuova si scrive dopo il controllo).
void controlla_riapertura(const Sorgente& chiuso, const Lettura& originale, const std::string& utente, const cripto::Segreto& password,
                          bool con_registro) {
    const std::optional<ManifestoLetto> m = leggi_manifesto(chiuso);
    const FileLetto f = chiuso.leggi(cassaforte::kFile, kMassimoByteFile);
    const FileLetto g = chiuso.leggi(registro::kFile, kMassimoByteFile);
    if (!m || f.esito != FileLetto::Esito::Letto || (con_registro && g.esito != FileLetto::Esito::Letto))
        throw ErroreKZero(Errore::ChiusuraNonVerificata, "Il pacchetto chiuso non si riesce a rileggere: niente e' stato scritto.");
    try {
        const cassaforte::Cassaforte c = cassaforte::Cassaforte::apri(f.testo, m->id, utente, password, *accesso::ambiente().politica);
        if (con_registro) registro::leggi(g.testo, c);
        const ValoriCassaforte v = chiusura::valori_di(c);
        const Lettura l = leggi_pacchetto(chiuso, &v);
        std::string differenza;
        if (!l.forma_valida() || !chiusura::stessi_dati(originale, l, differenza))
            throw ErroreKZero(Errore::ChiusuraNonVerificata,
                              "Riaperto, il pacchetto non da' gli stessi dati dell'originale" +
                                  (differenza.empty() ? std::string(" (problemi nella forma dei file)") : " (file " + differenza + ")") +
                                  ": di solito due elementi con lo stesso codice, che la cassaforte non saprebbe distinguere. "
                                  "Niente e' stato scritto; correggere i dati (kzero controlla) e riprovare.");
    } catch (const cassaforte::ErroreCassaforte& e) {
        throw ErroreKZero(Errore::ChiusuraNonVerificata, std::string("Il pacchetto chiuso non si riapre (") + e.what() + ").");
    } catch (const registro::ErroreRegistro& e) {
        throw ErroreKZero(Errore::ChiusuraNonVerificata, std::string("Il registro nuovo non si rilegge (") + e.what() + ").");
    }
}

void scrivi_normali(const fs::path& chiusa, const chiusura::Separazione& s, const std::string& id, std::uint32_t generazione) {
    for (const auto& [file, doc] : s.normali)
        if (file != "pacchetto.json") scrivi_o_errore(chiusa, file, chiusura::testo_file(doc));
    for (const FileDati& f : file_dati()) {
        std::error_code ec;
        if (!s.normali.count(f.file) && fs::is_regular_file(chiusa / f.file, ec)) {
            try {
                disco::togli(chiusa, f.file);
            } catch (const disco::ErroreDisco& e) {
                throw ErroreKZero(Errore::ScritturaNonRiuscita, e.what());
            }
        }
    }
    scrivi_o_errore(chiusa, "pacchetto.json", chiusura::testo_file(chiusura::manifesto_chiuso(s.normali.at("pacchetto.json"), id, generazione)));
}

}  // namespace kzero::api::riscrittura
