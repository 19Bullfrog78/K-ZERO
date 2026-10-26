// K-Zero API - il formato del file della cassaforte, byte per byte (vedi cassaforte_formato.hpp e docs/cassaforte.md).
#include "cassaforte_formato.hpp"

#include <algorithm>
#include <cstring>

#include "tempo.hpp"

namespace kzero::api::cassaforte::formato {

using cripto::Bytes;
using cripto::Segreto;
using Tipo = ErroreCassaforte::Tipo;


const char* nome_livello(int l) { return l == kBase ? "base" : l == kPersonali ? "dei dati personali" : "dei dati particolari"; }
bool ha(const Permessi& p, int livello) {
    return livello == kBase || (livello == kPersonali ? p.personali : p.particolari);
}
std::uint8_t bit(const Permessi& p) { return static_cast<std::uint8_t>((p.personali ? 1 : 0) | (p.particolari ? 2 : 0)); }
int livelli_di(const Permessi& p) { return 1 + (p.personali ? 1 : 0) + (p.particolari ? 1 : 0); }

[[noreturn]] void errore(Tipo t, const std::string& m) { throw ErroreCassaforte(t, m); }
[[noreturn]] void rovinata(const std::string& dove) {
    errore(Tipo::NonCassaforte, "Il file " + std::string(kFile) + " e' troncato o rovinato (" + dove +
                                    "): non si puo' aprire. Rimettere una copia buona del pacchetto.");
}
[[noreturn]] void manomessa(const std::string& cosa) {
    errore(Tipo::Manomessa, "La cassaforte e' stata cambiata da fuori di K-Zero (" + cosa +
                                "): non si apre e nessun dato si mostra. Rimettere una copia buona del pacchetto.");
}

bool codice_valido(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const bool alfanumerico = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alfanumerico && (i == 0 || (c != '-' && c != '_' && c != '.'))) return false;
    }
    return true;
}


// --- L'intestazione ----------------------------------------------------------------------------------------------------


std::size_t lunghezza_fessura(const Permessi& p) {
    return cripto::kSegretaBox + 1 + static_cast<std::size_t>(livelli_di(p)) * kVoceFessura + cripto::kEtichetta;
}
std::size_t lunghezza_storia(std::uint32_t versione) {
    return versione <= 1 ? 0 : cripto::kNonce + (versione - 1) * kVoceStoria + cripto::kEtichetta;
}

// Tutta l'intestazione fino alle autenticazioni comprese (con T_base com'e').
Bytes scrivi_intestazione(const Intestazione& i) {
    Scrittore w;
    w.byte(kMagia, sizeof kMagia);
    w.u16(kVersione);
    w.byte(i.id);
    w.u32(i.generazione);
    for (std::uint32_t v : i.versioni) w.u32(v);
    w.byte(reinterpret_cast<const unsigned char*>(i.conservazione.data()), i.conservazione.size());
    w.u32(i.registro_voci);
    w.byte(i.registro_impronta);
    w.u64(i.valori_personali);
    w.u64(i.valori_particolari);
    w.u16(static_cast<std::uint16_t>(i.utenti.size()));
    for (const Utente& u : i.utenti) {
        w.testo8(u.codice);
        w.u8(bit(u.permessi));
        w.byte(u.pubblica);
        w.byte(u.sale);
        w.u32(u.kdf.operazioni);
        w.u32(u.kdf.memoria_kib);
        w.byte(u.nonce);
        w.u16(static_cast<std::uint16_t>(u.fessura.size()));
        w.byte(u.fessura);
    }
    w.u16(static_cast<std::uint16_t>(i.buste.size()));
    for (const Busta& b : i.buste) {
        w.testo8(b.codice);
        w.u8(static_cast<std::uint8_t>(b.livello));
        w.u32(b.versione);
        w.byte(b.busta);
    }
    for (const Bytes& s : i.storie) {
        w.u32(static_cast<std::uint32_t>(s.size()));
        w.byte(s);
    }
    for (const Sezione& s : i.sezioni) {
        w.byte(s.nonce);
        w.u64(s.lunghezza);
        w.byte(s.impronta);
    }
    w.byte(i.autenticazione[kPersonali]);
    w.byte(i.autenticazione[kParticolari]);
    w.byte(i.autenticazione[kBase]);
    return std::move(w.dati());
}


// Legge l'intestazione, con tutti i controlli di forma (nessuna chiave serve).
Analizzato analizza(std::string_view file) {
    const auto* p = reinterpret_cast<const unsigned char*>(file.data());
    if (file.size() < sizeof kMagia || std::memcmp(p, kMagia, sizeof kMagia) != 0)
        errore(Tipo::NonCassaforte, "Il file " + std::string(kFile) +
                                        " non e' una cassaforte di K-Zero (vuoto, di un altro programma, o rovinato all'inizio).");
    Lettore r(p, file.size());
    r.byte(sizeof kMagia);
    const std::uint16_t versione = r.u16();
    if (!r.ok()) rovinata("versione");
    if (versione != kVersione)
        errore(Tipo::VersioneSconosciuta, "La cassaforte e' della versione " + std::to_string(versione) +
                                              " del formato, che questo programma non conosce (conosce la versione " +
                                              std::to_string(kVersione) + "): serve un programma K-Zero piu' recente.");
    Analizzato a;
    Intestazione& i = a.intestazione;
    i.id = r.byte(kId);
    i.generazione = r.u32();
    for (std::uint32_t& v : i.versioni) v = r.u32();
    const Bytes data = r.byte(10);
    i.conservazione.assign(data.begin(), data.end());
    i.registro_voci = r.u32();
    i.registro_impronta = r.byte(cripto::kImpronta);
    i.valori_personali = r.u64();
    i.valori_particolari = r.u64();
    if (!r.ok()) rovinata("intestazione");
    if (i.generazione == 0) rovinata("generazione");
    for (std::uint32_t v : i.versioni)
        if (v == 0 || v > kMassimoVersioni) rovinata("versioni delle chiavi");
    if (!kzero::day_from_text(i.conservazione)) rovinata("data di conservazione");

    const std::uint16_t n_utenti = r.u16();
    if (!r.ok() || n_utenti == 0 || n_utenti > kMassimoUtenti) rovinata("numero degli utenti");
    for (std::uint16_t k = 0; k < n_utenti; ++k) {
        Utente u;
        u.codice = r.testo8();
        const std::uint8_t b = r.u8();
        u.pubblica = r.byte(cripto::kPubblicaBox);
        u.sale = r.byte(cripto::kSale);
        u.kdf.operazioni = r.u32();
        u.kdf.memoria_kib = r.u32();
        u.nonce = r.byte(cripto::kNonce);
        const std::uint16_t lunghezza = r.u16();
        u.fessura = r.byte(lunghezza);
        if (!r.ok()) rovinata("utenti");
        if (!codice_valido(u.codice) || i.utente(u.codice)) rovinata("codice di un utente");
        if (b == 0 || b > 3) rovinata("permessi di un utente");
        u.permessi = Permessi{(b & 1) != 0, (b & 2) != 0};
        if (u.fessura.size() != lunghezza_fessura(u.permessi)) rovinata("fessura di un utente");
        i.utenti.push_back(std::move(u));
    }
    const std::uint16_t n_buste = r.u16();
    if (!r.ok() || n_buste > kMassimoBuste) rovinata("numero delle buste");
    for (std::uint16_t k = 0; k < n_buste; ++k) {
        Busta b;
        b.codice = r.testo8();
        b.livello = r.u8();
        b.versione = r.u32();
        b.busta = r.byte(kBusta);
        if (!r.ok()) rovinata("buste");
        if (!codice_valido(b.codice) || b.livello >= kLivelli || b.versione < 2 || b.versione > kMassimoVersioni)
            rovinata("busta");
        i.buste.push_back(std::move(b));
    }
    for (int l = 0; l < kLivelli; ++l) {
        const std::uint32_t n = r.u32();
        if (!r.ok() || n != lunghezza_storia(i.versioni[static_cast<std::size_t>(l)])) rovinata("storia delle chiavi");
        i.storie[static_cast<std::size_t>(l)] = r.byte(n);
    }
    for (Sezione& s : i.sezioni) {
        s.nonce = r.byte(cripto::kNonce);
        s.lunghezza = r.u64();
        s.impronta = r.byte(cripto::kImpronta);
        if (!r.ok() || s.lunghezza < cripto::kEtichetta || s.lunghezza > file.size()) rovinata("sezioni");
    }
    i.autenticazione[kPersonali] = r.byte(cripto::kAutenticazione);
    i.autenticazione[kParticolari] = r.byte(cripto::kAutenticazione);
    i.autenticazione[kBase] = r.byte(cripto::kAutenticazione);
    if (!r.ok()) rovinata("autenticazione");
    a.fine = r.posizione();
    std::uint64_t dati = 0;
    for (const Sezione& s : i.sezioni) dati += s.lunghezza;
    if (r.resto() != dati) rovinata("lunghezza del file");
    return a;
}

// --- Dati associati e messaggi autenticati ----------------------------------------------------------------------------

Bytes inizio(const Bytes& id) {
    Scrittore w;
    w.byte(kMagia, sizeof kMagia);
    w.u16(kVersione);
    w.byte(id);
    return std::move(w.dati());
}

Bytes ad_fessura(const Bytes& id, const Utente& u) {
    Scrittore w;
    w.byte(inizio(id));
    w.testo8(u.codice);
    w.u8(bit(u.permessi));
    w.byte(u.pubblica);
    w.byte(u.sale);
    w.u32(u.kdf.operazioni);
    w.u32(u.kdf.memoria_kib);
    return std::move(w.dati());
}

Bytes ad_livello(const Bytes& id, int livello, std::uint32_t versione) {
    Scrittore w;
    w.byte(inizio(id));
    w.u8(static_cast<std::uint8_t>(livello));
    w.u32(versione);
    return std::move(w.dati());
}

Bytes ad_sezione(const Bytes& id, int livello, std::uint32_t versione, std::uint32_t generazione) {
    Bytes ad = ad_livello(id, livello, versione);
    Scrittore w;
    w.u32(generazione);
    ad.insert(ad.end(), w.dati().begin(), w.dati().end());
    return ad;
}

Bytes ad_legati(const Bytes& id, std::uint32_t versione_personali, std::uint32_t versione_particolari, std::uint32_t generazione) {
    Scrittore w;
    w.byte(inizio(id));
    w.u8(static_cast<std::uint8_t>(kLegati));
    w.u32(versione_personali);
    w.u32(versione_particolari);
    w.u32(generazione);
    return std::move(w.dati());
}

// L'elenco di chi ha un livello (personali o particolari): codici e chiavi pubbliche, nell'ordine dell'intestazione.
Bytes lista(const Intestazione& i, int livello) {
    Scrittore w;
    w.byte(kMagiaLista, sizeof kMagiaLista);
    w.u16(kVersione);
    w.byte(i.id);
    w.u8(static_cast<std::uint8_t>(livello));
    w.u32(i.versioni[static_cast<std::size_t>(livello)]);
    std::vector<const Utente*> chi;
    for (const Utente& u : i.utenti)
        if (ha(u.permessi, livello)) chi.push_back(&u);
    w.u16(static_cast<std::uint16_t>(chi.size()));
    for (const Utente* u : chi) {
        w.testo8(u->codice);
        w.byte(u->pubblica);
    }
    return std::move(w.dati());
}

Bytes contenuto_busta(const Bytes& id, int livello, std::uint32_t versione, const Segreto& chiave) {
    Scrittore w;
    w.byte(id);
    w.u8(static_cast<std::uint8_t>(livello));
    w.u32(versione);
    w.byte(chiave.dati(), chiave.dimensione());
    return std::move(w.dati());
}

std::string testo_sezione(const std::vector<Voce>& voci) {
    JsonDati elenco = JsonDati::array();
    for (const Voce& v : voci) elenco.push_back(v.in_json());
    return elenco.dump(-1, ' ', false, JsonDati::error_handler_t::strict);
}

// Le voci di una sezione decifrata. false se la forma non e' quella attesa (solo chi ha la chiave puo' averla scritta:
// e' comunque un file da non usare).
bool voci_di(std::string_view testo, Classe classe, std::vector<Voce>& out, bool legato) {
    JsonDati elenco;
    try {
        const JsonDati::parser_callback_t limite = [](int profondita, JsonDati::parse_event_t, JsonDati&) {
            if (profondita > kProfonditaSezione) throw std::runtime_error("troppo profondo");
            return true;
        };
        elenco = JsonDati::parse(testo.begin(), testo.end(), limite);
    } catch (const std::exception&) {
        return false;
    }
    if (!elenco.is_array()) return false;
    std::vector<Voce> voci;
    voci.reserve(elenco.size());
    for (const JsonDati& j : elenco) {
        Voce v;
        if (!Voce::da_json(j, classe, v)) return false;
        v.legato = legato;
        voci.push_back(std::move(v));
    }
    out = std::move(voci);
    return true;
}


cripto::Segreto derivata(const cripto::Segreto& chiave_livello, std::uint64_t uso) {
    return cripto::derivata(chiave_livello, uso, kContesto);
}

cripto::Segreto chiave_legati(const Bytes& id, const cripto::Segreto& chiave_personali, const cripto::Segreto& chiave_particolari) {
    return cripto::combinata(derivata(chiave_personali, kPerLegati), derivata(chiave_particolari, kPerLegati), inizio(id),
                             std::string_view(kContestoLegati));
}

}  // namespace kzero::api::cassaforte::formato
