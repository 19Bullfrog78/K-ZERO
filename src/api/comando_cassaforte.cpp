// K-Zero API - i comandi della cassaforte dei dati personali (K-010): chiudi, richiudi, esporta, cassaforte, registro,
// aggiungi-utente, togli-utente, password, conservazione, cancella. Uscita: schema "cassaforte" (il registro: "registro").
// Ogni comando che scrive o cambia qualcosa lo dichiara con --scrivi <cartella> (nessun comando scrive senza): la cartella
// nuova di una chiusura o di un'uscita in chiaro, oppure la cartella del pacchetto chiuso che cambia (DECISIONI.md, K-010).
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accesso.hpp"
#include "chiusura.hpp"
#include "disco.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "formato_dati.hpp"
#include "riscrittura.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;
using riscrittura::chiuso_in_memoria;
using riscrittura::controlla_riapertura;
using riscrittura::scrivi_o_errore;

// --- Uscite ------------------------------------------------------------------------------------------------------------

std::string testo_classi(bool personali, bool particolari) {
    return cassaforte::testo_permessi_breve(cassaforte::Permessi{personali, particolari});
}

struct Esito {
    std::string comando;
    std::string utente;
    std::string pacchetto_codice, pacchetto_nome;
    std::optional<cassaforte::Riepilogo> cassaforte;
    std::optional<std::size_t> voci_registro;
    bool scaduta = false;
    std::string fatto;
    // Prima le indicazioni del comando e i suoi avvisi del catalogo (es. K-CTR-099), poi quelli di ogni apertura (K-CTR-097,
    // K-CTR-098): aggiungi_avvisi_dell_apertura.
    std::vector<accesso::AvvisoMostrato> avvisi;
    Json cancellazione = nullptr;
};

Json riepilogo_json(const cassaforte::Riepilogo& r, bool scaduta, std::size_t voci) {
    Json j = Json::object();
    j["id"] = r.id;
    j["generazione"] = r.generazione;
    j["conservazione"] = r.conservazione;
    j["conservazione_scaduta"] = scaduta;
    j["valori"] = {{"personali", r.valori.personali}, {"particolari", r.valori.particolari}};
    Json utenti = Json::array();
    for (const cassaforte::UtenteVisibile& u : r.utenti)
        utenti.push_back({{"codice", u.codice}, {"permessi", accesso::classi(u.permessi.personali, u.permessi.particolari)}});
    j["utenti"] = utenti;
    j["voci_registro"] = voci;
    return j;
}

// Gli avvisi, uguali in testo e in JSON: codice K-CTR (null per le indicazioni del comando) e messaggio.
Json avvisi_json(const std::vector<accesso::AvvisoMostrato>& avvisi) {
    Json out = Json::array();
    for (const accesso::AvvisoMostrato& a : avvisi)
        out.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
    return out;
}

std::string avvisi_testo(const std::vector<accesso::AvvisoMostrato>& avvisi) {
    std::string t;
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return t;
}

void aggiungi_avvisi_dell_apertura(Esito& e) {
    if (!e.cassaforte) return;
    for (const kzero::Problem& p : accesso::avvisi_dell_apertura(*e.cassaforte, e.utente)) e.avvisi.push_back(accesso::mostra(p));
}

Risultato uscita(const DefinizioneComando& comando, const Parametri& par, const Esito& e) {
    const std::size_t voci = e.voci_registro.value_or(e.cassaforte ? e.cassaforte->registro_voci : 0);
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = e.comando;
        j["utente"] = e.utente;
        j["pacchetto"] = {{"codice", e.pacchetto_codice}, {"nome", e.pacchetto_nome}};
        j["cassaforte"] = e.cassaforte ? riepilogo_json(*e.cassaforte, e.scaduta, voci) : Json(nullptr);
        j["fatto"] = e.fatto;
        j["avvisi"] = avvisi_json(e.avvisi);
        j["cancellazione"] = e.cancellazione;
        return {testo_json(j), 0};
    }
    std::string t = e.fatto + "\n";
    if (e.cassaforte) {
        const cassaforte::Riepilogo& r = *e.cassaforte;
        t += "Cassaforte " + r.id + " del pacchetto " + e.pacchetto_codice + ", chiusura numero " + std::to_string(r.generazione) +
             "; conservazione fino al " + r.conservazione + (e.scaduta ? " (data passata)" : "") + ".\n";
        t += "Valori: " + std::to_string(r.valori.personali) + " personali e " + std::to_string(r.valori.particolari) +
             " particolari (mai mostrati qui).\n";
        std::string u;
        for (const cassaforte::UtenteVisibile& x : r.utenti)
            u += (u.empty() ? "" : ", ") + x.codice + " (" + testo_classi(x.permessi.personali, x.permessi.particolari) + ")";
        t += "Utenti: " + u + ".\n";
        t += "Registro degli accessi: " + std::to_string(voci) + (voci == 1 ? " voce.\n" : " voci.\n");
    }
    t += avvisi_testo(e.avvisi);
    return {t, 0};
}

bool scaduta(const cassaforte::Riepilogo& r) { return accesso::conservazione_passata(r); }

// --- Strumenti ----------------------------------------------------------------------------------------------------------

void cartella_esistente(const std::string& cartella) {
    std::error_code ec;
    if (cartella.empty() || !fs::is_directory(fs::path(cartella), ec))
        throw ErroreKZero(Errore::CartellaNonTrovata, "Cartella del pacchetto non trovata: '" + cartella +
                                                          "'. Indicare la cartella che contiene il manifesto pacchetto.json.");
}

void cartella_nuova(const std::string& cartella) {
    std::error_code ec;
    if (fs::exists(fs::symlink_status(fs::path(cartella), ec)))
        throw ErroreKZero(Errore::CartellaGiaPresente, "La cartella '" + cartella + "' c'e' gia': il comando scrive sempre in una "
                                                       "cartella nuova, per non mescolare dati in chiaro e dati chiusi.");
    const fs::path padre = fs::absolute(fs::path(cartella), ec).parent_path();
    if (ec || !fs::is_directory(padre, ec))
        throw ErroreKZero(Errore::ScritturaNonRiuscita, "La cartella che deve contenere '" + cartella + "' non c'e'.");
}

// Una cartella temporanea accanto a quella da creare: si scrive tutto li', si controlla, poi la si rinomina con una sola
// operazione. Un'interruzione lascia al piu' la cartella temporanea (nome che comincia con "."), mai una cartella a meta'.
class CartellaTemporanea {
public:
    explicit CartellaTemporanea(const fs::path& destinazione) : destinazione_(fs::absolute(destinazione)) {
        percorso_ = destinazione_.parent_path() /
                    ("." + destinazione_.filename().string() + ".kzero-" + cripto::esadecimale(cripto::casuali(8)));
        std::error_code ec;
        if (!fs::create_directory(percorso_, ec) || ec)
            throw ErroreKZero(Errore::ScritturaNonRiuscita, "Non si riesce a creare una cartella accanto a '" +
                                                                destinazione.string() + "' (permessi o disco).");
    }
    ~CartellaTemporanea() {
        if (!finita_) {
            std::error_code ec;
            fs::remove_all(percorso_, ec);
        }
    }
    const fs::path& percorso() const { return percorso_; }
    void concludi() {
        std::error_code ec;
        fs::rename(percorso_, destinazione_, ec);
        if (ec) throw ErroreKZero(Errore::ScritturaNonRiuscita, "La cartella '" + destinazione_.string() + "' non si riesce a creare.");
        finita_ = true;
    }

private:
    fs::path destinazione_;
    fs::path percorso_;
    bool finita_ = false;
};

// Il pacchetto in chiaro da chiudere o richiudere: letto, senza errori di forma, manifesto della versione 2 senza cassaforte.
Lettura in_chiaro(const std::string& cartella) {
    cartella_esistente(cartella);
    Lettura l = leggi_pacchetto(CartellaSuDisco(cartella));
    if (!l.usato("pacchetto.json") || !l.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto in chiaro ha errori nella forma dei file: prima vanno corretti "
                                                      "(kzero controlla " + cartella + ").");
    if (l.versioni.at("pacchetto.json") < 2)
        throw ErroreKZero(Errore::ManifestoDaAggiornare,
                          "Il manifesto del pacchetto e' della versione 1: per chiuderlo va portato alla versione 2, che dichiara "
                          "la cassaforte e la scelta porta_a_porta_sensibile (docs/formato-dati.md). La scelta e' dell'utente.");
    if (l.cassaforte_dichiarata || l.cassaforte_presente)
        throw ErroreKZero(Errore::PacchettoDiverso, "Il pacchetto in chiaro dichiara gia' una cassaforte (o ne ha il file): si "
                                                    "chiude o si richiude solo un pacchetto in chiaro.");
    return l;
}

std::vector<std::string> contesto_password(const std::string& utente, const std::string& pacchetto) {
    return {utente, pacchetto};
}

cassaforte::Permessi permessi_da(const std::string& p) {
    if (p == "tutti") return {true, true};
    if (p == "particolari") return {false, true};
    return {true, false};
}

}  // namespace

// --- chiudi -------------------------------------------------------------------------------------------------------------

Risultato esegui_chiudi(const DefinizioneComando& comando, const Parametri& par) {
    const std::string& sorgente = par.argomenti.at(0);
    const Lettura originale = in_chiaro(sorgente);
    cartella_nuova(par.scrivi);
    const kzero::Manifest& m = originale.pacchetto.manifest;
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password_nuova(*in, par.utente, contesto_password(par.utente, m.code));
    cripto::niente_core_dump();
    const std::string conservazione = kzero::text_from_day(*par.conservazione);

    chiusura::Separazione s = chiusura::separa(originale);
    const ValoriPerClasse valori = s.contenuto.valori;
    const std::string id = cassaforte::nuovo_id();
    const accesso::Ambiente& amb = accesso::ambiente();
    cassaforte::Cassaforte c =
        cassaforte::Cassaforte::crea(id, conservazione, par.utente, password, *amb.politica, std::move(s.contenuto));
    registro::Registro r = registro::nuovo(id);
    registro::Voce v;
    v.ora = amb.orologio();
    v.utente = par.utente;
    v.comando = "chiudi";
    v.aperte = accesso::classi(true, true);
    v.nota = "pacchetto chiuso: " + std::to_string(valori.personali) + " valori personali e " + std::to_string(valori.particolari) +
             " particolari nella cassaforte; riaperto e controllato";
    const registro::Voce prima = registro::aggiungi(r, v);
    c.lega_registro(prima.numero, registro::impronta(prima));
    const PacchettoInMemoria chiuso = chiuso_in_memoria(s, id, 1, c.file(), registro::scrivi(r, c));

    CartellaTemporanea temporanea(par.scrivi);
    for (const auto& [nome, testo] : chiuso.file) scrivi_o_errore(temporanea.percorso(), nome, testo);
    controlla_riapertura(CartellaSuDisco(temporanea.percorso()), originale, par.utente, password, true);
    temporanea.concludi();

    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = m.code;
    e.pacchetto_nome = m.name;
    e.cassaforte = c.riepilogo();
    e.voci_registro = r.voci.size();
    e.scaduta = scaduta(*e.cassaforte);
    e.fatto = "Pacchetto " + m.code + " chiuso nella cartella nuova: file normali senza dati personali, cassaforte cifrata e "
              "registro degli accessi; riaperto e controllato, i dati sono identici all'originale. L'utente " + par.utente +
              " ha i dati personali e particolari.";
    e.avvisi.push_back({"", "La cartella in chiaro '" + sorgente + "' non e' stata toccata: va cancellata a mano. Copie vecchie "
                            "dei dati in chiaro possono restare sul disco (e nelle copie di sicurezza): serve anche la cifratura "
                            "del disco (docs/sicurezza.md)."});
    aggiungi_avvisi_dell_apertura(e);
    return uscita(comando, par, e);
}

// --- richiudi -----------------------------------------------------------------------------------------------------------

Risultato esegui_richiudi(const DefinizioneComando& comando, const Parametri& par) {
    const std::string& sorgente = par.argomenti.at(0);
    const fs::path chiusa(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(chiusa);
    const Lettura originale = in_chiaro(sorgente);
    if (originale.pacchetto.manifest.code != manifesto.codice)
        throw ErroreKZero(Errore::PacchettoDiverso, "Il pacchetto in chiaro e' " + originale.pacchetto.manifest.code +
                                                        ", quello chiuso " + manifesto.codice + ": si richiude solo l'uscita in "
                                                        "chiaro dello stesso pacchetto.");
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    chiusura::Separazione s = chiusura::separa(originale);
    const ValoriPerClasse valori = s.contenuto.valori;
    std::uint32_t generazione = 0;
    const cassaforte::Riepilogo r = accesso::cambia(
        chiusa, manifesto, par.utente, password, comando.nome,
        [&](cassaforte::Cassaforte& c) {
            c.sostituisci_contenuto(std::move(s.contenuto));
            generazione = c.riepilogo().generazione;
            // Prima di scrivere: il pacchetto nuovo, in memoria, si riapre con la password e da' gli stessi dati.
            controlla_riapertura(chiuso_in_memoria(s, manifesto.id, generazione, c.file(), ""), originale, par.utente, password,
                                 false);
            return "dati richiusi: " + std::to_string(valori.personali) + " valori personali e " + std::to_string(valori.particolari) +
                   " particolari (chiusura numero " + std::to_string(generazione) + ")";
        },
        accesso::ambiente());
    // Poi i file normali (il manifesto per ultimo, con il numero della chiusura): un'interruzione a meta' si vede (K-CTR-093)
    // e si ripara rifacendo la richiusura.
    riscrittura::scrivi_normali(chiusa, s, manifesto.id, generazione);

    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = manifesto.codice;
    e.pacchetto_nome = originale.pacchetto.manifest.name;
    e.cassaforte = r;
    e.scaduta = scaduta(r);
    e.fatto = "Pacchetto " + manifesto.codice + " richiuso: dati sostituiti nella stessa cassaforte (chiusura numero " +
              std::to_string(generazione) + "), utenti e registro tenuti; controllato prima di scrivere.";
    e.avvisi.push_back({"", "La cartella in chiaro '" + sorgente + "' va cancellata a mano adesso."});
    aggiungi_avvisi_dell_apertura(e);
    return uscita(comando, par, e);
}

// --- esporta ------------------------------------------------------------------------------------------------------------

Risultato esegui_esporta(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path chiusa(par.argomenti.at(0));
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(chiusa);
    cartella_nuova(par.scrivi);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    // Un'uscita in chiaro serve a richiudere i dati corretti: deve contenerli tutti (DECISIONI.md, K-010).
    std::vector<cassaforte::UtenteVisibile> utenti;   // dall'intestazione non ancora autenticata: solo per fermarsi subito
    try {
        utenti = cassaforte::leggi_intestazione(leggi_dal_disco(chiusa, cassaforte::kFile, kMassimoByteFile).testo).utenti;
    } catch (const cassaforte::ErroreCassaforte&) {
        // lo dira' l'apertura
    }
    for (const cassaforte::UtenteVisibile& u : utenti)
        if (u.codice == par.utente && !u.permessi.tutti())
            throw ErroreKZero(Errore::PermessoMancante, "L'uscita in chiaro serve a correggere e richiudere tutti i dati: la puo' "
                                                        "fare solo chi ha i dati personali e particolari (l'utente " + par.utente +
                                                        " ha " + cassaforte::testo_permessi(u.permessi) + ").");
    accesso::Apertura a = accesso::apri(chiusa, manifesto, par.utente, password, comando.nome, Visibilita{true, true},
                                        "uscita in chiaro di tutti i dati in una cartella nuova", accesso::ambiente());
    if (!a.cassaforte.permessi().tutti())
        throw ErroreKZero(Errore::PermessoMancante, "L'uscita in chiaro la puo' fare solo chi ha i dati personali e particolari.");
    const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
    const Lettura l = leggi_pacchetto(CartellaSuDisco(chiusa), &v);
    if (!l.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto chiuso ha errori nella forma dei file: prima vanno corretti "
                                                      "(kzero controlla " + chiusa.string() + " --utente " + par.utente + ").");
    CartellaTemporanea temporanea(par.scrivi);
    for (const auto& [file, doc] : l.documenti)
        scrivi_o_errore(temporanea.percorso(), file,
                        chiusura::testo_file(file == "pacchetto.json" ? chiusura::manifesto_in_chiaro(doc) : doc));
    temporanea.concludi();

    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = manifesto.codice;
    e.pacchetto_nome = l.pacchetto.manifest.name;
    e.cassaforte = a.cassaforte.riepilogo();
    e.voci_registro = a.registro.voci.size();
    e.scaduta = scaduta(*e.cassaforte);
    e.fatto = "Uscita in chiaro del pacchetto " + manifesto.codice + " scritta nella cartella nuova, con tutti i dati personali e "
              "particolari; scritta nel registro.";
    e.avvisi.push_back({"", "La cartella '" + par.scrivi + "' contiene i dati in chiaro: correggerli, richiuderli (kzero "
                            "richiudi " + par.scrivi + " --scrivi " + chiusa.string() + " --utente " + par.utente +
                            ") e cancellarla subito. Copie vecchie possono restare sul disco (cifratura del disco, "
                            "docs/sicurezza.md)."});
    aggiungi_avvisi_dell_apertura(e);
    return uscita(comando, par, e);
}

// --- cassaforte e registro -------------------------------------------------------------------------------------------------

Risultato esegui_cassaforte(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const accesso::Apertura a =
        accesso::apri(cartella, manifesto, par.utente, password, comando.nome, Visibilita{}, "", accesso::ambiente());
    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = manifesto.codice;
    e.pacchetto_nome = manifesto.documento.value("nome", "");
    e.cassaforte = a.cassaforte.riepilogo();
    e.voci_registro = a.registro.voci.size();
    e.scaduta = scaduta(*e.cassaforte);
    e.fatto = "Cassaforte del pacchetto " + manifesto.codice + " aperta dall'utente " + par.utente + " (" +
              cassaforte::testo_permessi_breve(a.cassaforte.permessi()) + ").";
    aggiungi_avvisi_dell_apertura(e);
    return uscita(comando, par, e);
}

Risultato esegui_registro(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome, Visibilita{},
                                              "lettura del registro", accesso::ambiente());
    const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
    const bool passata = scaduta(r);
    std::vector<accesso::AvvisoMostrato> avvisi;
    for (const kzero::Problem& p : accesso::avvisi_dell_apertura(r, par.utente)) avvisi.push_back(accesso::mostra(p));
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["utente"] = par.utente;
        j["pacchetto"] = {{"codice", manifesto.codice}, {"nome", manifesto.documento.value("nome", "")}};
        j["cassaforte"] = r.id;
        j["catena_controllata"] = true;
        j["conservazione"] = r.conservazione;
        j["conservazione_scaduta"] = passata;
        Json voci = Json::array();
        for (const registro::Voce& v : a.registro.voci)
            voci.push_back({{"numero", v.numero},
                            {"ora", v.ora},
                            {"utente", v.utente},
                            {"comando", v.comando},
                            {"classi_aperte", v.aperte},
                            {"classi_mostrate", v.mostrate},
                            {"nota", v.nota}});
        j["voci"] = voci;
        j["avvisi"] = avvisi_json(avvisi);
        return {testo_json(j), 0};
    }
    std::string t = "Registro degli accessi alla cassaforte del pacchetto " + manifesto.codice + " (" +
                    std::to_string(a.registro.voci.size()) + " voci; catena controllata; ore in UTC, dall'orologio del PC).\n";
    for (const registro::Voce& v : a.registro.voci) {
        t += std::to_string(v.numero) + ". " + v.ora + "  " + v.utente + "  " + v.comando + "  aperte: " +
             (v.aperte.empty() ? std::string("nessuna") : testo_classi(std::count(v.aperte.begin(), v.aperte.end(), "personali") > 0,
                                                         std::count(v.aperte.begin(), v.aperte.end(), "particolari") > 0)) +
             "; mostrate: " +
             (v.mostrate.empty() ? std::string("nessuna") : testo_classi(std::count(v.mostrate.begin(), v.mostrate.end(), "personali") > 0,
                                                           std::count(v.mostrate.begin(), v.mostrate.end(), "particolari") > 0));
        if (!v.nota.empty()) t += "; " + v.nota;
        t += "\n";
    }
    t += avvisi_testo(avvisi);
    return {t, 0};
}

// --- utenti, password, conservazione ---------------------------------------------------------------------------------------

namespace {

// avviso = un avviso del catalogo proprio del comando (es. K-CTR-099 dopo aver tolto un utente), prima di quelli
// dell'apertura; r = la cassaforte dopo il cambiamento.
Risultato dopo_cambio(const DefinizioneComando& comando, const Parametri& par, const ManifestoLetto& manifesto,
                      const cassaforte::Riepilogo& r, const std::string& fatto,
                      const std::optional<kzero::Problem>& avviso = std::nullopt) {
    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = manifesto.codice;
    e.pacchetto_nome = manifesto.documento.value("nome", "");
    e.cassaforte = r;
    e.scaduta = scaduta(r);
    e.fatto = fatto;
    if (avviso) e.avvisi.push_back(accesso::mostra(*avviso));
    aggiungi_avvisi_dell_apertura(e);
    return uscita(comando, par, e);
}

}  // namespace

Risultato esegui_aggiungi_utente(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const cassaforte::Permessi permessi = permessi_da(par.permessi);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    const cripto::Segreto nuova =
        accesso::chiedi_password_nuova(*in, par.nuovo_utente, contesto_password(par.nuovo_utente, manifesto.codice));
    cripto::niente_core_dump();
    const std::string cosa = "l'utente " + par.nuovo_utente + " con " + cassaforte::testo_permessi(permessi);
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, par.utente, password, comando.nome,
        [&](cassaforte::Cassaforte& c) {
            c.aggiungi_utente(par.nuovo_utente, permessi, nuova, *accesso::ambiente().politica);
            return "aggiunto " + cosa;
        },
        accesso::ambiente());
    return dopo_cambio(comando, par, manifesto, r, "Aggiunto " + cosa + ".",
                       accesso::avviso(kzero::Check::PasswordWrittenByOther, 0, {{"nuovo", par.nuovo_utente}, {"chi", par.utente}}));
}

Risultato esegui_togli_utente(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, par.utente, password, comando.nome,
        [&](cassaforte::Cassaforte& c) {
            c.togli_utente(par.utente_da_togliere);
            return "tolto l'utente " + par.utente_da_togliere + ": chiavi dei suoi dati rifatte e dati ricifrati";
        },
        accesso::ambiente());
    return dopo_cambio(comando, par, manifesto, r,
                       "Tolto l'utente " + par.utente_da_togliere + ": le chiavi dei dati che aveva sono state rifatte e i dati "
                       "ricifrati; non apre piu' questa versione della cassaforte. Le copie vecchie del file restano leggibili "
                       "con le chiavi di allora (docs/sicurezza.md).",
                       accesso::avviso(kzero::Check::UserRemovedPasswords, 0, {{"tolto", par.utente_da_togliere}, {"utente", par.utente}}));
}

Risultato esegui_password(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const std::string per = par.per_utente.empty() ? par.utente : par.per_utente;
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    const cripto::Segreto nuova = accesso::chiedi_password_nuova(*in, per, contesto_password(per, manifesto.codice));
    cripto::niente_core_dump();
    const std::string cosa = per == par.utente ? "password cambiata dall'utente " + per : "password nuova per l'utente " + per;
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, par.utente, password, comando.nome,
        [&](cassaforte::Cassaforte& c) {
            c.cambia_password(per, nuova, *accesso::ambiente().politica);
            return cosa;
        },
        accesso::ambiente());
    std::optional<kzero::Problem> scritta_da_altri;
    if (per != par.utente)
        scritta_da_altri = accesso::avviso(kzero::Check::PasswordWrittenByOther, 1, {{"nuovo", per}, {"chi", par.utente}});
    return dopo_cambio(comando, par, manifesto, r, "Fatto: " + cosa + ". La password vecchia non apre piu' questa versione.",
                       scritta_da_altri);
}

Risultato esegui_conservazione(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const std::string data = kzero::text_from_day(*par.conservazione);
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, par.utente, password, comando.nome,
        [&](cassaforte::Cassaforte& c) {
            c.cambia_conservazione(data);
            return "data di conservazione: " + data;
        },
        accesso::ambiente());
    return dopo_cambio(comando, par, manifesto, r, "Data di conservazione dei dati cambiata: " + data + ".");
}

// --- cancella -----------------------------------------------------------------------------------------------------------

Risultato esegui_cancella(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const accesso::Ambiente& amb = accesso::ambiente();
    std::unique_ptr<disco::Blocco> blocco;
    try {
        blocco = std::make_unique<disco::Blocco>(cartella);
    } catch (const disco::ErroreDisco& e) {
        throw ErroreKZero(Errore::ScritturaNonRiuscita, e.what());
    }
    const FileLetto f = leggi_dal_disco(cartella, cassaforte::kFile, kMassimoByteFile);
    if (f.esito != FileLetto::Esito::Letto)
        throw ErroreKZero(Errore::CassaforteIlleggibile, "La cassaforte non si riesce a leggere: non si cancella niente.");
    std::optional<cassaforte::Cassaforte> c;
    try {
        c.emplace(cassaforte::Cassaforte::apri(f.testo, manifesto.id, par.utente, password, *amb.politica));
    } catch (const cassaforte::ErroreCassaforte& e) {
        accesso::rilancia(e);
    }
    if (!c->permessi().tutti())
        throw ErroreKZero(Errore::PermessoMancante, "Cancellare la cassaforte distrugge tutti i dati: lo puo' fare solo chi ha i dati "
                                                    "personali e particolari (l'utente " + par.utente + " ha " +
                                                    cassaforte::testo_permessi(c->permessi()) + ").");
    // Il registro sparisce con la cassaforte: il resoconto lo dice, letto prima (se si legge).
    std::size_t voci = 0;
    {
        const FileLetto g = leggi_dal_disco(cartella, registro::kFile, kMassimoByteFile);
        try {
            if (g.esito == FileLetto::Esito::Letto) voci = registro::leggi(g.testo, *c).voci.size();
        } catch (const registro::ErroreRegistro&) {
            voci = 0;
        }
    }
    const cassaforte::Riepilogo r = c->riepilogo();
    const std::string ora = amb.orologio();
    // Prima le chiavi: l'intestazione con le fessure si sovrascrive con zeri, poi i file.
    try {
        disco::azzera_inizio(cartella, cassaforte::kFile, cassaforte::lunghezza_intestazione(f.testo));
        disco::togli(cartella, cassaforte::kFile);
        disco::togli(cartella, registro::kFile);
    } catch (const disco::ErroreDisco& e) {
        throw ErroreKZero(Errore::ScritturaNonRiuscita, std::string("Cancellazione non completata: ") + e.what() + ".");
    }
    Esito e;
    e.comando = comando.nome;
    e.utente = par.utente;
    e.pacchetto_codice = manifesto.codice;
    e.pacchetto_nome = manifesto.documento.value("nome", "");
    e.fatto = "Cassaforte del pacchetto " + manifesto.codice + " cancellata il " + ora + " (UTC) dall'utente " + par.utente +
              ": distrutte prima le fessure con le chiavi, poi i file della cassaforte e del registro (" + std::to_string(voci) +
              " voci). Conteneva " + std::to_string(r.valori.personali) + " valori personali e " +
              std::to_string(r.valori.particolari) + " particolari, di " + std::to_string(r.utenti.size()) +
              (r.utenti.size() == 1 ? " utente." : " utenti.") + " Conservare questo resoconto.";
    e.avvisi.push_back({"", "Le copie vecchie della cassaforte (sul disco, nelle copie di sicurezza, presso un utente tolto) "
                            "restano, cifrate con le password di allora (docs/sicurezza.md). Nella cartella restano i file "
                            "normali, senza dati personali: si possono cancellare."});
    e.cancellazione = {{"ora", ora},
                       {"cassaforte", r.id},
                       {"valori", {{"personali", r.valori.personali}, {"particolari", r.valori.particolari}}},
                       {"utenti", r.utenti.size()},
                       {"voci_registro", voci}};
    return uscita(comando, par, e);
}

}  // namespace kzero::api
