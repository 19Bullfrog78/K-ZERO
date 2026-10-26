// K-Zero API - aprire e cambiare la cassaforte di una cartella (vedi accesso.hpp).
#include "accesso.hpp"

#include <system_error>

#include "chiusura.hpp"
#include "controlli_catalogo.hpp"
#include "disco.hpp"
#include "errori.hpp"

namespace kzero::api::accesso {

namespace {

namespace fs = std::filesystem;

cripto::Segreto leggi_riga(password::Ingresso& in, const std::string& domanda) {
    try {
        return in.leggi(domanda);
    } catch (const std::runtime_error&) {
        throw ErroreKZero(Errore::PasswordNonDisponibile,
                          "L'ingresso e' finito prima della password richiesta (" + domanda.substr(0, domanda.find(':')) +
                              "): con --password-da-ingresso va scritta una password per riga, nell'ordine in cui il comando "
                              "le chiede.");
    }
}

std::string leggi_file(const fs::path& cartella, const std::string& nome, Errore se_manca, const std::string& cosa) {
    const FileLetto f = leggi_dal_disco(cartella, nome, kMassimoByteFile);
    if (f.esito != FileLetto::Esito::Letto)
        throw ErroreKZero(se_manca, cosa + " (" + nome + ") non si riesce a leggere: manca, e' troppo grande o non e' un file "
                                         "normale. Nessun dato si apre.");
    return f.testo;
}

registro::Registro leggi_registro(const fs::path& cartella, const cassaforte::Cassaforte& c) {
    const std::string testo = leggi_file(cartella, registro::kFile, Errore::RegistroIlleggibile, "Il registro degli accessi");
    try {
        return registro::leggi(testo, c);
    } catch (const registro::ErroreRegistro& e) {
        throw ErroreKZero(Errore::RegistroIlleggibile, e.what());
    }
}

void scrivi(const fs::path& cartella, const std::string& nome, const std::string& contenuto, const std::string& cosa) {
    try {
        disco::scrivi_atomico(cartella, nome, contenuto);
    } catch (const disco::ErroreDisco& e) {
        throw ErroreKZero(Errore::ScritturaNonRiuscita, cosa + " non si riesce a scrivere: " + e.what() +
                                                            ". Il file di prima e' rimasto com'era.");
    }
}

}  // namespace

namespace {
const Ambiente* ambiente_prove = nullptr;
}  // namespace

const Ambiente& ambiente() {
    static const Ambiente programma{};
    return ambiente_prove ? *ambiente_prove : programma;
}

void per_le_prove_imposta_ambiente(const Ambiente* a) { ambiente_prove = a; }

std::unique_ptr<password::Ingresso> ingresso(const Parametri& parametri) {
    if (ambiente().ingresso) return ambiente().ingresso();
    if (parametri.password_da_ingresso) return password::dall_ingresso_standard();
    std::unique_ptr<password::Ingresso> t = password::dal_terminale();
    if (!t)
        throw ErroreKZero(Errore::PasswordNonDisponibile,
                          "Serve una password, ma il programma non ha un terminale da cui chiederla. Lanciarlo da un terminale, "
                          "oppure passare le password sull'ingresso standard, una per riga, con --password-da-ingresso (mai come "
                          "parametro).");
    return t;
}

cripto::Segreto chiedi_password(password::Ingresso& in, const std::string& utente) {
    const cripto::Segreto scritta = leggi_riga(in, "Password dell'utente " + utente + ": ");
    password::Problema problema = password::Problema::Nessuno;
    cripto::Segreto n = password::normalizza(scritta, problema);
    if (problema != password::Problema::Nessuno)
        throw ErroreKZero(Errore::PasswordSbagliata, "La password scritta per l'utente " + utente + " non e' valida (" +
                                                         password::spiegazione(problema, 0) + "): non apre la cassaforte.");
    return n;
}

cripto::Segreto chiedi_password_nuova(password::Ingresso& in, const std::string& utente, const std::vector<std::string>& contesto) {
    const cripto::Segreto prima =
        leggi_riga(in, "Password nuova per l'utente " + utente + " (almeno 15 caratteri: una frase va benissimo): ");
    if (in.terminale()) {
        const cripto::Segreto seconda = leggi_riga(in, "Riscrivi la password nuova per conferma: ");
        if (prima.dimensione() != seconda.dimensione() || !cripto::uguali(prima.dati(), seconda.dati(), prima.dimensione()))
            throw ErroreKZero(Errore::PasswordNonAmmessa, "Le due password scritte non coincidono: niente e' stato cambiato.");
    }
    password::Problema problema = password::Problema::Nessuno;
    std::size_t caratteri = 0;
    cripto::Segreto n = password::nuova(prima, contesto, problema, caratteri);
    if (problema != password::Problema::Nessuno)
        throw ErroreKZero(Errore::PasswordNonAmmessa, "Password nuova non ammessa per l'utente " + utente + ": " +
                                                          password::spiegazione(problema, caratteri) +
                                                          ". Niente e' stato cambiato.");
    return n;
}

ManifestoLetto manifesto_con_cassaforte(const fs::path& cartella) {
    std::error_code ec;
    if (cartella.empty() || !fs::is_directory(cartella, ec))
        throw ErroreKZero(Errore::CartellaNonTrovata, "Cartella del pacchetto non trovata: '" + cartella.string() +
                                                          "'. Indicare la cartella che contiene il manifesto pacchetto.json.");
    const std::optional<ManifestoLetto> m = leggi_manifesto(CartellaSuDisco(cartella));
    if (!m)
        throw ErroreKZero(Errore::PacchettoConErrori, "Il manifesto pacchetto.json manca o ha errori: prima va corretto (kzero "
                                                      "controlla " + cartella.string() + ").");
    if (!m->cassaforte)
        throw ErroreKZero(Errore::PacchettoSenzaCassaforte, "Il pacchetto " + m->codice + " non ha una cassaforte: i suoi dati sono "
                                                            "in chiaro (per chiuderlo: kzero chiudi).");
    if (!fs::exists(cartella / cassaforte::kFile, ec))
        throw ErroreKZero(Errore::CassaforteMancante, "Il manifesto del pacchetto " + m->codice +
                                                          " dichiara una cassaforte, ma il file cassaforte.kzc non c'e': rimetterlo "
                                                          "dalla stessa copia del pacchetto.");
    return *m;
}

std::vector<std::string> classi(bool personali, bool particolari) {
    std::vector<std::string> c;
    if (personali) c.push_back("personali");
    if (particolari) c.push_back("particolari");
    return c;
}

void rilancia(const cassaforte::ErroreCassaforte& e) {
    using T = cassaforte::ErroreCassaforte::Tipo;
    switch (e.tipo) {
        case T::NonCassaforte: throw ErroreKZero(Errore::CassaforteIlleggibile, e.what());
        case T::VersioneSconosciuta: throw ErroreKZero(Errore::CassaforteVersioneSconosciuta, e.what());
        case T::AltroPacchetto: throw ErroreKZero(Errore::CassaforteDiUnAltroPacchetto, e.what());
        case T::UtenteSconosciuto: throw ErroreKZero(Errore::UtenteSconosciuto, e.what());
        case T::ParametriNonAmmessi: throw ErroreKZero(Errore::ParametriCassaforteNonAmmessi, e.what());
        case T::PasswordSbagliata: throw ErroreKZero(Errore::PasswordSbagliata, e.what());
        case T::Manomessa: throw ErroreKZero(Errore::CassaforteManomessa, e.what());
        case T::PermessoMancante: throw ErroreKZero(Errore::PermessoMancante, e.what());
        case T::UtentiNonAmmessi: throw ErroreKZero(Errore::UtentiNonAmmessi, e.what());
    }
    throw ErroreKZero(Errore::ErroreInterno, "Errore della cassaforte senza codice.");
}

namespace {

// Blocca la cartella, legge e apre la cassaforte e il registro: cio' che hanno in comune apri e cambia.
struct Aperte {
    std::unique_ptr<disco::Blocco> blocco;
    std::optional<cassaforte::Cassaforte> cassaforte;
    registro::Registro registro;
};

Aperte apri_sotto_blocco(const fs::path& cartella, const ManifestoLetto& m, const std::string& utente, const cripto::Segreto& password,
                         const Ambiente& ambiente) {
    Aperte a;
    try {
        a.blocco = std::make_unique<disco::Blocco>(cartella);
    } catch (const disco::ErroreDisco& e) {
        throw ErroreKZero(Errore::ScritturaNonRiuscita, std::string(e.what()) + ": senza il blocco non si apre la cassaforte.");
    }
    const std::string file = leggi_file(cartella, cassaforte::kFile, Errore::CassaforteIlleggibile, "La cassaforte");
    try {
        a.cassaforte.emplace(cassaforte::Cassaforte::apri(file, m.id, utente, password, *ambiente.politica));
    } catch (const cassaforte::ErroreCassaforte& e) {
        rilancia(e);
    }
    a.registro = leggi_registro(cartella, *a.cassaforte);
    return a;
}

registro::Voce voce(const Ambiente& ambiente, const std::string& utente, const std::string& comando,
                    const cassaforte::Permessi& aperte, const Visibilita& mostrate, const std::string& nota) {
    registro::Voce v;
    v.ora = ambiente.orologio();
    v.utente = utente;
    v.comando = comando;
    v.aperte = classi(aperte.personali, aperte.particolari);
    v.mostrate = classi(mostrate.personali, mostrate.particolari);
    v.nota = nota;
    return v;
}

}  // namespace

Apertura apri(const fs::path& cartella, const ManifestoLetto& manifesto, const std::string& utente, const cripto::Segreto& password,
              const std::string& comando, const Visibilita& mostrate, const std::string& nota, const Ambiente& ambiente) {
    Aperte a = apri_sotto_blocco(cartella, manifesto, utente, password, ambiente);
    cassaforte::Cassaforte& c = *a.cassaforte;
    const cassaforte::Permessi p = c.permessi();
    if ((mostrate.personali && !p.personali) || (mostrate.particolari && !p.particolari))
        throw std::logic_error("accesso: classi mostrate oltre i permessi (da controllare prima)");
    registro::aggiungi(a.registro, voce(ambiente, utente, comando, p, mostrate, nota));
    // Prima la voce del registro, poi i dati: se la voce non si scrive, nessun dato.
    scrivi(cartella, registro::kFile, registro::scrivi(a.registro, c), "La voce del registro degli accessi");
    return Apertura{std::move(c), std::move(a.registro)};
}

cassaforte::Riepilogo cambia(const fs::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
                             const cripto::Segreto& password, const std::string& comando,
                             const std::function<std::string(cassaforte::Cassaforte&)>& cambia, const Ambiente& ambiente) {
    Aperte a = apri_sotto_blocco(cartella, manifesto, utente, password, ambiente);
    cassaforte::Cassaforte& c = *a.cassaforte;
    const std::uint32_t base_prima = c.versione_base();
    const cassaforte::Permessi aperte = c.permessi();
    std::string nota;
    try {
        nota = cambia(c);
    } catch (const cassaforte::ErroreCassaforte& e) {
        rilancia(e);
    }
    const registro::Voce v = registro::aggiungi(a.registro, voce(ambiente, utente, comando, aperte, Visibilita{}, nota));
    // 1. la voce, con la chiave base di prima (la cassaforte nuova la conserva nella sua storia);
    scrivi(cartella, registro::kFile, registro::scrivi(a.registro, c, base_prima), "La voce del registro degli accessi");
    // 2. la cassaforte, che ricorda l'ultima voce;
    c.lega_registro(v.numero, registro::impronta(v));
    scrivi(cartella, cassaforte::kFile, c.file(), "La cassaforte");
    // 3. con le chiavi nuove, il registro ricifrato: chi e' stato tolto non lo legge piu'.
    if (c.versione_base() != base_prima)
        scrivi(cartella, registro::kFile, registro::scrivi(a.registro, c), "Il registro degli accessi ricifrato");
    return c.riepilogo();
}

LetturaPerComando leggi_per_comando(const std::string& cartella, const Parametri& parametri, const std::string& comando,
                                    const std::string& nota) {
    std::error_code ec;
    if (cartella.empty() || !fs::is_directory(fs::path(cartella), ec))
        throw ErroreKZero(Errore::CartellaNonTrovata, "Cartella del pacchetto non trovata: '" + cartella +
                                                          "'. Indicare la cartella che contiene il manifesto pacchetto.json.");
    const CartellaSuDisco sorgente(cartella);
    LetturaPerComando out;
    out.visibilita = Visibilita{parametri.includi_dati_personali, parametri.includi_dati_particolari};
    const std::optional<ManifestoLetto> m = leggi_manifesto(sorgente);
    const bool con_cassaforte = m && m->cassaforte && fs::exists(fs::path(cartella) / cassaforte::kFile, ec);
    if (!con_cassaforte) {
        if (!parametri.utente.empty() && !(m && m->cassaforte))
            throw ErroreKZero(Errore::PacchettoSenzaCassaforte,
                              "Il pacchetto non ha una cassaforte: --utente non serve (i suoi dati sono in chiaro).");
        // Cassaforte dichiarata ma mancante: lo dice la lettura (K-CTR-091), senza chiedere nulla.
        out.lettura = leggi_pacchetto(sorgente);
        return out;
    }
    if (parametri.utente.empty())
        throw ErroreKZero(Errore::ServeUtente, "Il pacchetto " + m->codice + " ha i dati personali nella cassaforte: per aprirla "
                                               "serve --utente <codice> (la password si chiede dopo).");
    out.utente = parametri.utente;
    const auto controlla_permessi = [&](const cassaforte::Permessi& p) {
        const auto manca = [&](bool chiesto, bool permesso, const char* parametro, const char* dati) {
            if (chiesto && !permesso)
                throw ErroreKZero(Errore::PermessoMancante, std::string(parametro) + ": l'utente " + parametri.utente +
                                                                " non ha il permesso dei dati " + dati + " (ha " +
                                                                cassaforte::testo_permessi(p) + "). Niente e' stato mostrato.");
        };
        manca(parametri.includi_dati_personali, p.personali, "--includi-dati-personali", "personali");
        manca(parametri.includi_dati_particolari, p.particolari, "--includi-dati-particolari", "particolari");
    };
    // Prima della password, con l'intestazione non ancora autenticata: per fermarsi subito. Dopo l'apertura, di nuovo.
    try {
        const FileLetto f = leggi_dal_disco(cartella, cassaforte::kFile, kMassimoByteFile);
        if (f.esito == FileLetto::Esito::Letto)
            for (const cassaforte::UtenteVisibile& u : cassaforte::leggi_intestazione(f.testo).utenti)
                if (u.codice == parametri.utente) controlla_permessi(u.permessi);
    } catch (const cassaforte::ErroreCassaforte&) {
        // lo dira' l'apertura
    }
    const std::unique_ptr<password::Ingresso> in = ingresso(parametri);
    const cripto::Segreto password = chiedi_password(*in, parametri.utente);
    cripto::niente_core_dump();
    Apertura a = apri(cartella, *m, parametri.utente, password, comando, out.visibilita, nota, ambiente());
    out.permessi = a.cassaforte.permessi();
    controlla_permessi(out.permessi);
    out.cassaforte = a.cassaforte.riepilogo();
    const ValoriCassaforte valori = chiusura::valori_di(a.cassaforte);
    out.lettura = leggi_pacchetto(sorgente, &valori);
    out.scaduta = conservazione_passata(*out.cassaforte);
    out.avvisi = avvisi_dell_apertura(*out.cassaforte, out.utente);
    for (const kzero::Problem& p : out.avvisi) out.lettura.problemi.push_back(p);
    return out;
}

bool conservazione_passata(const cassaforte::Riepilogo& r) { return ambiente().orologio().substr(0, 10) > r.conservazione; }

kzero::Problem avviso(kzero::Check controllo, int variante, std::vector<std::pair<std::string, std::string>> parametri) {
    kzero::Problem p{controllo, variante, kzero::Source{"", "", 0, 0}, {}};
    for (auto& [nome, valore] : parametri) p.params.push_back(kzero::text_param(std::move(nome), std::move(valore)));
    return p;
}

std::vector<kzero::Problem> avvisi_dell_apertura(const cassaforte::Riepilogo& r, const std::string& utente) {
    std::vector<kzero::Problem> out;
    if (conservazione_passata(r)) out.push_back(avviso(kzero::Check::RetentionExpired, 0, {{"data", r.conservazione}}));
    if (r.chiavi_in_busta) out.push_back(avviso(kzero::Check::KeysFromEnvelope, 0, {{"utente", utente}}));
    return out;
}

AvvisoMostrato mostra(const kzero::Problem& p) {
    const Presentatore presentatore(nullptr, nullptr, Visibilita{});
    return {definizione_controllo(p.check).codice, presentatore.messaggio(p)};
}

}  // namespace kzero::api::accesso
