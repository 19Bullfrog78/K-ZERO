// K-Zero API - la cassaforte dei dati personali (vedi cassaforte.hpp e docs/cassaforte.md, formato byte per byte).
#include "cassaforte.hpp"

#include "cassaforte_formato.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

#include "tempo.hpp"

namespace kzero::api::cassaforte {

using cripto::Bytes;
using cripto::Segreto;
using Tipo = ErroreCassaforte::Tipo;

using namespace formato;

std::string testo_permessi(const Permessi& p) {
    if (p.tutti()) return "i dati personali e particolari";
    if (p.personali) return "i dati personali";
    if (p.particolari) return "i dati particolari";
    return "nessun dato";
}

std::string nuovo_id() { return cripto::esadecimale(cripto::casuali(kId)); }

// --- Lo stato di una cassaforte aperta ------------------------------------------------------------------------------------

struct Cassaforte::Stato {
    Intestazione i;
    std::array<Bytes, 2> cifrate;                            // testo cifrato delle sezioni
    Segreto kek;                                             // dalla password dell'utente che l'ha aperta
    Segreto segreta;                                         // la sua chiave X25519
    std::array<Segreto, kLivelli> chiavi;                    // chiavi di adesso dei suoi livelli (vuote per gli altri)
    std::array<std::map<std::uint32_t, Segreto>, kLivelli> vecchie;   // chiavi vecchie dei suoi livelli (dalla storia)
    std::array<std::vector<Voce>, 2> voci;                   // le sezioni che puo' leggere

    const Utente& io(const std::string& codice) const {
        const Utente* u = i.utente(codice);
        if (!u) throw std::logic_error("cassaforte: utente aperto non trovato");
        return *u;
    }

    // Fessura di un utente con le chiavi di adesso dei suoi livelli (chi la scrive le ha tutte).
    void scrivi_fessura(Utente& u, const Segreto& kek_utente, const Segreto& segreta_utente) {
        Segreto chiaro(lunghezza_fessura(u.permessi) - cripto::kEtichetta);
        std::size_t k = 0;
        const auto metti = [&](const unsigned char* p, std::size_t n) {
            std::memcpy(chiaro.dati() + k, p, n);
            k += n;
        };
        metti(segreta_utente.dati(), cripto::kSegretaBox);
        const unsigned char n = static_cast<unsigned char>(livelli_di(u.permessi));
        metti(&n, 1);
        for (int l = 0; l < kLivelli; ++l) {
            if (!ha(u.permessi, l)) continue;
            const Segreto& chiave = chiavi[static_cast<std::size_t>(l)];
            if (chiave.dimensione() != cripto::kChiave) throw std::logic_error("cassaforte: chiave di un livello mancante");
            Scrittore w;
            w.u8(static_cast<std::uint8_t>(l));
            w.u32(i.versioni[static_cast<std::size_t>(l)]);
            metti(w.dati().data(), w.dati().size());
            metti(chiave.dati(), chiave.dimensione());
        }
        chiaro.usa(k);
        u.nonce = cripto::casuali(cripto::kNonce);
        u.fessura = cripto::cifra(kek_utente, u.nonce.data(), chiaro.dati(), chiaro.dimensione(), ad_fessura(i.id, u));
    }

    void cifra_sezione(int classe) {   // classe: 0 personali, 1 particolari
        const int livello = classe == 0 ? kPersonali : kParticolari;
        const Segreto chiave = cripto::derivata(chiavi[static_cast<std::size_t>(livello)], kPerSezione, kContesto);
        Sezione& s = i.sezioni[static_cast<std::size_t>(classe)];
        const std::string testo = testo_sezione(voci[static_cast<std::size_t>(classe)]);
        s.nonce = cripto::casuali(cripto::kNonce);
        Bytes& c = cifrate[static_cast<std::size_t>(classe)];
        c = cripto::cifra(chiave, s.nonce.data(), reinterpret_cast<const unsigned char*>(testo.data()), testo.size(),
                          ad_sezione(i.id, livello, i.versioni[static_cast<std::size_t>(livello)], i.generazione));
        s.lunghezza = c.size();
        s.impronta = cripto::impronta(c);
    }

    void autentica_lista(int livello) {
        const Segreto chiave = cripto::derivata(chiavi[static_cast<std::size_t>(livello)], kPerAutenticazione, kContesto);
        i.autenticazione[static_cast<std::size_t>(livello)] = cripto::autentica(chiave, lista(i, livello));
    }

    // La storia di un livello: le chiavi vecchie, cifrate con quella di adesso.
    void scrivi_storia(int livello) {
        const std::size_t l = static_cast<std::size_t>(livello);
        if (i.versioni[l] <= 1) {
            i.storie[l].clear();
            return;
        }
        Segreto chiaro((i.versioni[l] - 1) * kVoceStoria);
        std::size_t k = 0;
        for (std::uint32_t v = 1; v < i.versioni[l]; ++v) {
            const auto it = vecchie[l].find(v);
            if (it == vecchie[l].end()) throw std::logic_error("cassaforte: chiave vecchia mancante");
            Scrittore w;
            w.u32(v);
            std::memcpy(chiaro.dati() + k, w.dati().data(), 4);
            std::memcpy(chiaro.dati() + k + 4, it->second.dati(), cripto::kChiave);
            k += kVoceStoria;
        }
        chiaro.usa(k);
        const Segreto chiave = cripto::derivata(chiavi[l], kPerStoria, kContesto);
        const Bytes nonce = cripto::casuali(cripto::kNonce);
        Bytes s = nonce;
        const Bytes c = cripto::cifra(chiave, nonce.data(), chiaro.dati(), chiaro.dimensione(), ad_livello(i.id, livello, i.versioni[l]));
        s.insert(s.end(), c.begin(), c.end());
        i.storie[l] = std::move(s);
    }

    // Legge la storia di un livello con la chiave di adesso (false se non e' autentica).
    bool leggi_storia(int livello) {
        const std::size_t l = static_cast<std::size_t>(livello);
        vecchie[l].clear();
        if (i.versioni[l] <= 1) return true;
        const Bytes& s = i.storie[l];
        const Segreto chiave = cripto::derivata(chiavi[l], kPerStoria, kContesto);
        Segreto chiaro;
        if (!cripto::decifra(chiave, s.data(), s.data() + cripto::kNonce, s.size() - cripto::kNonce,
                             ad_livello(i.id, livello, i.versioni[l]), chiaro))
            return false;
        Lettore r(chiaro.dati(), chiaro.dimensione());
        for (std::uint32_t v = 1; v < i.versioni[l]; ++v) {
            if (r.u32() != v) return false;
            const Bytes chiave_vecchia = r.byte(cripto::kChiave);
            if (!r.ok()) return false;
            vecchie[l].emplace(v, Segreto(chiave_vecchia.data(), chiave_vecchia.size()));
            Bytes copia = chiave_vecchia;
            cripto::azzera(copia.data(), copia.size());
        }
        return r.ok() && r.resto() == 0;
    }

    Bytes intestazione_autenticata() const {
        Intestazione copia = i;
        Bytes h = scrivi_intestazione(copia);
        const Segreto chiave = cripto::derivata(chiavi[kBase], kPerAutenticazione, kContesto);
        const Bytes prima(h.begin(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
        const Bytes t = cripto::autentica(chiave, prima);
        std::copy(t.begin(), t.end(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
        return h;
    }
};

Cassaforte::Cassaforte() : s_(std::make_unique<Stato>()) {}
Cassaforte::Cassaforte(Cassaforte&&) noexcept = default;
Cassaforte& Cassaforte::operator=(Cassaforte&&) noexcept = default;
Cassaforte::~Cassaforte() = default;

namespace {

Riepilogo riepilogo_di(const Intestazione& i) {
    Riepilogo r;
    r.id = cripto::esadecimale(i.id);
    r.generazione = i.generazione;
    r.conservazione = i.conservazione;
    r.valori.personali = static_cast<std::size_t>(i.valori_personali);
    r.valori.particolari = static_cast<std::size_t>(i.valori_particolari);
    for (const Utente& u : i.utenti) r.utenti.push_back({u.codice, u.permessi, u.kdf});
    r.registro_voci = i.registro_voci;
    r.registro_impronta = i.registro_impronta;
    return r;
}

}  // namespace

Riepilogo leggi_intestazione(std::string_view file) { return riepilogo_di(analizza(file).intestazione); }

Cassaforte Cassaforte::apri(std::string_view file, const std::string& id_atteso, const std::string& utente,
                            const Segreto& password, const cripto::PoliticaKdf& politica) {
    cripto::avvia();
    Analizzato a = analizza(file);
    Cassaforte c;
    Stato& s = *c.s_;
    s.i = std::move(a.intestazione);
    Intestazione& i = s.i;
    if (cripto::esadecimale(i.id) != id_atteso)
        errore(Tipo::AltroPacchetto, "La cassaforte non e' di questo pacchetto (il suo codice non e' quello scritto nel "
                                     "manifesto): forse e' stata copiata da un'altra cartella. Rimettere la cassaforte giusta.");
    const Utente* u = i.utente(utente);
    if (!u) errore(Tipo::UtenteSconosciuto, "L'utente " + utente + " non e' tra gli utenti della cassaforte.");
    switch (cripto::controlla_parametri(u->kdf, politica)) {
        case cripto::EsitoParametri::Ammessi: break;
        case cripto::EsitoParametri::TroppoBassi:
            errore(Tipo::ParametriNonAmmessi, "La fessura dell'utente " + utente +
                                                  " ha parametri della password sotto il minimo di questo programma: "
                                                  "non si apre (parametri abbassati da fuori di K-Zero?).");
        case cripto::EsitoParametri::Assurdi:
            errore(Tipo::ParametriNonAmmessi, "La fessura dell'utente " + utente +
                                                  " chiede parametri della password assurdi (es. memoria enorme): non si "
                                                  "calcola nulla e la cassaforte non si apre.");
    }
    s.kek = cripto::deriva(password, u->sale.data(), u->kdf);
    Segreto chiaro;
    if (!cripto::decifra(s.kek, u->nonce.data(), u->fessura.data(), u->fessura.size(), ad_fessura(i.id, *u), chiaro))
        errore(Tipo::PasswordSbagliata, "Password sbagliata per l'utente " + utente +
                                            " (oppure la sua fessura e' stata cambiata da fuori di K-Zero).");
    // Da qui: la fessura e' autentica (dati associati: codice, permessi, chiave pubblica, sale e parametri).
    Lettore r(chiaro.dati(), chiaro.dimensione());
    {
        const Bytes segreta = r.byte(cripto::kSegretaBox);
        if (!r.ok()) manomessa("fessura");
        s.segreta = Segreto(segreta.data(), segreta.size());
        Bytes copia = segreta;
        cripto::azzera(copia.data(), copia.size());
    }
    if (cripto::pubblica_box(s.segreta) != u->pubblica) manomessa("chiave pubblica dell'utente");
    const std::uint8_t n = r.u8();
    if (n != livelli_di(u->permessi)) manomessa("fessura");
    for (int l = 0; l < kLivelli; ++l) {
        if (!ha(u->permessi, l)) continue;
        const std::size_t ls = static_cast<std::size_t>(l);
        const std::uint8_t livello = r.u8();
        const std::uint32_t ancora = r.u32();
        const Bytes chiave = r.byte(cripto::kChiave);
        if (!r.ok() || livello != l) manomessa("fessura");
        Segreto chiave_ancora(chiave.data(), chiave.size());
        {
            Bytes copia = chiave;
            cripto::azzera(copia.data(), copia.size());
        }
        const std::uint32_t adesso = i.versioni[ls];
        if (ancora > adesso) manomessa("versione della chiave " + std::string(nome_livello(l)) + " abbassata");
        if (ancora == adesso) {
            s.chiavi[ls] = std::move(chiave_ancora);
            if (!s.leggi_storia(l)) manomessa("storia delle chiavi " + std::string(nome_livello(l)));
            continue;
        }
        // La chiave e' stata rifatta: arriva in una busta, e vale solo se la sua storia contiene la chiave della fessura.
        const auto b = std::find_if(i.buste.begin(), i.buste.end(), [&](const Busta& x) {
            return x.codice == utente && x.livello == l && x.versione == adesso;
        });
        if (b == i.buste.end()) manomessa("manca la busta con la chiave " + std::string(nome_livello(l)) + " nuova");
        Segreto in_busta;
        if (!cripto::apri_busta(b->busta, u->pubblica, s.segreta, in_busta) || in_busta.dimensione() != kInBusta)
            manomessa("busta");
        Lettore rb(in_busta.dati(), in_busta.dimensione());
        const Bytes id = rb.byte(kId);
        const std::uint8_t lb = rb.u8();
        const std::uint32_t vb = rb.u32();
        const Bytes nuova = rb.byte(cripto::kChiave);
        if (!rb.ok() || id != i.id || lb != l || vb != adesso) manomessa("busta");
        s.chiavi[ls] = Segreto(nuova.data(), nuova.size());
        {
            Bytes copia = nuova;
            cripto::azzera(copia.data(), copia.size());
        }
        if (!s.leggi_storia(l)) manomessa("storia delle chiavi " + std::string(nome_livello(l)));
        const auto vecchia = s.vecchie[ls].find(ancora);
        if (vecchia == s.vecchie[ls].end() ||
            !cripto::uguali(vecchia->second.dati(), chiave_ancora.dati(), cripto::kChiave))
            manomessa("chiave " + std::string(nome_livello(l)) + " nuova non preparata da chi aveva quella vecchia");
    }
    if (r.resto() != 0) manomessa("fessura");
    // L'intestazione intera, con la chiave base; gli elenchi di chi ha i dati personali o particolari, con quelle chiavi.
    {
        const Bytes h = scrivi_intestazione(i);
        const Bytes prima(h.begin(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
        if (prima.size() + cripto::kAutenticazione != a.fine ||
            !std::equal(prima.begin(), prima.end(), reinterpret_cast<const unsigned char*>(file.data())))
            manomessa("intestazione");
        const Segreto chiave = cripto::derivata(s.chiavi[kBase], kPerAutenticazione, kContesto);
        if (!cripto::verifica_autenticazione(i.autenticazione[kBase], prima, chiave)) manomessa("intestazione");
    }
    for (int l : {kPersonali, kParticolari}) {
        if (!ha(u->permessi, l)) continue;
        const Segreto chiave = cripto::derivata(s.chiavi[static_cast<std::size_t>(l)], kPerAutenticazione, kContesto);
        if (!cripto::verifica_autenticazione(i.autenticazione[static_cast<std::size_t>(l)], lista(i, l), chiave))
            manomessa(std::string("elenco di chi ha i dati ") + (l == kPersonali ? "personali" : "particolari"));
    }
    // Sezioni: l'impronta di tutte e due (autenticata con l'intestazione), il contenuto di quelle dei suoi permessi.
    std::size_t inizio_sezione = a.fine;
    for (int classe = 0; classe < 2; ++classe) {
        const std::size_t cs = static_cast<std::size_t>(classe);
        const Sezione& sez = i.sezioni[cs];
        const auto* p = reinterpret_cast<const unsigned char*>(file.data()) + inizio_sezione;
        s.cifrate[cs] = Bytes(p, p + sez.lunghezza);
        inizio_sezione += static_cast<std::size_t>(sez.lunghezza);
        if (cripto::impronta(s.cifrate[cs]) != sez.impronta) manomessa("sezione");
        const int livello = classe == 0 ? kPersonali : kParticolari;
        if (!ha(u->permessi, livello)) continue;
        const Segreto chiave = cripto::derivata(s.chiavi[static_cast<std::size_t>(livello)], kPerSezione, kContesto);
        Segreto testo;
        if (!cripto::decifra(chiave, sez.nonce.data(), s.cifrate[cs].data(), s.cifrate[cs].size(),
                             ad_sezione(i.id, livello, i.versioni[static_cast<std::size_t>(livello)], i.generazione), testo))
            manomessa("sezione dei dati");
        if (!voci_di(testo.testo(), classe == 0 ? Classe::Personale : Classe::Particolare, s.voci[cs]))
            manomessa("contenuto della sezione dei dati");
    }
    c.utente_ = utente;
    return c;
}

Cassaforte Cassaforte::crea(const std::string& id, const std::string& conservazione, const std::string& utente,
                            const Segreto& password, const cripto::PoliticaKdf& politica, Contenuto contenuto) {
    cripto::avvia();
    if (!codice_valido(utente)) throw std::logic_error("cassaforte: codice dell'utente non valido");
    if (!kzero::day_from_text(conservazione)) throw std::logic_error("cassaforte: data di conservazione non valida");
    Cassaforte c;
    Stato& s = *c.s_;
    Intestazione& i = s.i;
    if (!cripto::da_esadecimale(id, i.id) || i.id.size() != kId) throw std::logic_error("cassaforte: codice non valido");
    i.conservazione = conservazione;
    i.valori_personali = contenuto.valori.personali;
    i.valori_particolari = contenuto.valori.particolari;
    for (Segreto& k : s.chiavi) {
        const Bytes nuova = cripto::casuali(cripto::kChiave);
        k = Segreto(nuova.data(), nuova.size());
    }
    cripto::CoppiaBox coppia = cripto::nuova_coppia_box();
    Utente u;
    u.codice = utente;
    u.permessi = Permessi{true, true};
    u.pubblica = coppia.pubblica;
    u.sale = cripto::casuali(cripto::kSale);
    u.kdf = politica.nuovi;
    s.kek = cripto::deriva(password, u.sale.data(), u.kdf);
    s.segreta = std::move(coppia.segreta);
    s.scrivi_fessura(u, s.kek, s.segreta);
    i.utenti.push_back(std::move(u));
    s.voci[0] = std::move(contenuto.personali);
    s.voci[1] = std::move(contenuto.particolari);
    s.cifra_sezione(0);
    s.cifra_sezione(1);
    s.autentica_lista(kPersonali);
    s.autentica_lista(kParticolari);
    c.utente_ = utente;
    return c;
}

Permessi Cassaforte::permessi() const { return s_->io(utente_).permessi; }

Riepilogo Cassaforte::riepilogo() const { return riepilogo_di(s_->i); }

const std::vector<Voce>& Cassaforte::voci(Classe classe) const {
    return s_->voci[classe == Classe::Particolare ? 1 : 0];
}

std::uint32_t Cassaforte::versione_base() const { return s_->i.versioni[kBase]; }

Segreto Cassaforte::chiave_registro(std::uint32_t versione_base) const {
    if (versione_base == s_->i.versioni[kBase]) return cripto::derivata(s_->chiavi[kBase], kPerRegistro, kContesto);
    const auto it = s_->vecchie[kBase].find(versione_base);
    if (it == s_->vecchie[kBase].end()) manomessa("registro di una versione della chiave base che non c'e'");
    return cripto::derivata(it->second, kPerRegistro, kContesto);
}

void Cassaforte::aggiungi_utente(const std::string& codice, const Permessi& permessi, const Segreto& password,
                                 const cripto::PoliticaKdf& politica) {
    Intestazione& i = s_->i;
    if (!codice_valido(codice)) throw std::logic_error("cassaforte: codice dell'utente non valido");
    if (permessi.nessuno()) throw std::logic_error("cassaforte: utente senza permessi");
    if (i.utente(codice)) errore(Tipo::UtentiNonAmmessi, "L'utente " + codice + " c'e' gia' nella cassaforte.");
    if (i.utenti.size() >= kMassimoUtenti)
        errore(Tipo::UtentiNonAmmessi, "La cassaforte ha gia' " + std::to_string(kMassimoUtenti) + " utenti, il massimo.");
    if (!this->permessi().contiene(permessi))
        errore(Tipo::PermessoMancante, "Si possono dare al massimo i permessi che si hanno: l'utente " + utente_ + " ha " +
                                           testo_permessi(this->permessi()) + ", non " + testo_permessi(permessi) + ".");
    cripto::CoppiaBox coppia = cripto::nuova_coppia_box();
    Utente u;
    u.codice = codice;
    u.permessi = permessi;
    u.pubblica = coppia.pubblica;
    u.sale = cripto::casuali(cripto::kSale);
    u.kdf = politica.nuovi;
    const Segreto kek = cripto::deriva(password, u.sale.data(), u.kdf);
    s_->scrivi_fessura(u, kek, coppia.segreta);
    i.utenti.push_back(std::move(u));
    if (permessi.personali) s_->autentica_lista(kPersonali);
    if (permessi.particolari) s_->autentica_lista(kParticolari);
}

void Cassaforte::togli_utente(const std::string& codice) {
    Intestazione& i = s_->i;
    const Utente* via = i.utente(codice);
    if (!via) errore(Tipo::UtenteSconosciuto, "L'utente " + codice + " non e' tra gli utenti della cassaforte.");
    const Permessi suoi = via->permessi;
    if (i.utenti.size() == 1)
        errore(Tipo::UtentiNonAmmessi, "L'utente " + codice + " e' l'ultimo della cassaforte: non si puo' togliere (per "
                                       "togliere tutto c'e' kzero cancella).");
    if (!permessi().contiene(suoi))
        errore(Tipo::PermessoMancante, "Per togliere l'utente " + codice + ", che ha " + testo_permessi(suoi) +
                                           ", servono tutti i suoi permessi: l'utente " + utente_ + " ha " +
                                           testo_permessi(permessi()) + ".");
    for (int l : {kPersonali, kParticolari}) {
        if (!ha(suoi, l)) continue;
        const bool altri = std::any_of(i.utenti.begin(), i.utenti.end(),
                                       [&](const Utente& u) { return u.codice != codice && ha(u.permessi, l); });
        if (!altri)
            errore(Tipo::UtentiNonAmmessi, "L'utente " + codice + " e' l'ultimo che legge i dati " +
                                               (l == kPersonali ? "personali" : "particolari") +
                                               ": togliendolo nessuno li potrebbe piu' aprire. Prima aggiungere un altro "
                                               "utente con quel permesso (per togliere tutto c'e' kzero cancella).");
    }
    const bool me_stesso = codice == utente_;
    // Via l'utente e le sue buste.
    i.utenti.erase(std::remove_if(i.utenti.begin(), i.utenti.end(), [&](const Utente& u) { return u.codice == codice; }),
                   i.utenti.end());
    // Chiavi nuove per i livelli che aveva: le vecchie vanno nella storia, le nuove in una busta per chi ha quel livello.
    for (int l = 0; l < kLivelli; ++l) {
        if (!ha(suoi, l)) continue;
        const std::size_t ls = static_cast<std::size_t>(l);
        if (i.versioni[ls] >= kMassimoVersioni)
            errore(Tipo::UtentiNonAmmessi, "Le chiavi della cassaforte sono gia' state rifatte " +
                                               std::to_string(kMassimoVersioni) + " volte, il massimo.");
        s_->vecchie[ls].emplace(i.versioni[ls], std::move(s_->chiavi[ls]));
        const Bytes nuova = cripto::casuali(cripto::kChiave);
        s_->chiavi[ls] = Segreto(nuova.data(), nuova.size());
        i.versioni[ls] += 1;
        s_->scrivi_storia(l);
        i.buste.erase(std::remove_if(i.buste.begin(), i.buste.end(), [&](const Busta& b) { return b.livello == l; }),
                      i.buste.end());
        for (const Utente& u : i.utenti) {
            if (u.codice == utente_ || !ha(u.permessi, l)) continue;
            // L'intestazione e' gia' verificata (apertura): u e la sua chiave pubblica sono autentici.
            const Bytes dentro = contenuto_busta(i.id, l, i.versioni[ls], s_->chiavi[ls]);
            Busta b;
            b.codice = u.codice;
            b.livello = l;
            b.versione = i.versioni[ls];
            b.busta = cripto::sigilla(dentro.data(), dentro.size(), u.pubblica);
            Bytes copia = dentro;
            cripto::azzera(copia.data(), copia.size());
            i.buste.push_back(std::move(b));
        }
    }
    i.buste.erase(std::remove_if(i.buste.begin(), i.buste.end(), [&](const Busta& b) { return b.codice == codice; }),
                  i.buste.end());
    if (suoi.personali) s_->cifra_sezione(0);
    if (suoi.particolari) s_->cifra_sezione(1);
    if (suoi.personali) s_->autentica_lista(kPersonali);
    if (suoi.particolari) s_->autentica_lista(kParticolari);
    if (!me_stesso) {
        // La propria fessura con le chiavi nuove: le proprie buste non servono piu'.
        Utente* io = i.utente(utente_);
        s_->scrivi_fessura(*io, s_->kek, s_->segreta);
        i.buste.erase(std::remove_if(i.buste.begin(), i.buste.end(), [&](const Busta& b) { return b.codice == utente_; }),
                      i.buste.end());
    }
}

void Cassaforte::cambia_password(const std::string& codice, const Segreto& password, const cripto::PoliticaKdf& politica) {
    Intestazione& i = s_->i;
    Utente* u = i.utente(codice);
    if (!u) errore(Tipo::UtenteSconosciuto, "L'utente " + codice + " non e' tra gli utenti della cassaforte.");
    if (codice == utente_) {
        u->sale = cripto::casuali(cripto::kSale);
        u->kdf = politica.nuovi;
        s_->kek = cripto::deriva(password, u->sale.data(), u->kdf);
        s_->scrivi_fessura(*u, s_->kek, s_->segreta);
    } else {
        // Per un altro utente: chi da' la password nuova ha tutti i suoi permessi; la sua chiave segreta non si conosce,
        // quindi l'utente riceve una coppia X25519 nuova.
        if (!permessi().contiene(u->permessi))
            errore(Tipo::PermessoMancante, "Per dare una password nuova all'utente " + codice + ", che ha " +
                                               testo_permessi(u->permessi) + ", servono tutti i suoi permessi: l'utente " +
                                               utente_ + " ha " + testo_permessi(permessi()) + ".");
        cripto::CoppiaBox coppia = cripto::nuova_coppia_box();
        u->pubblica = coppia.pubblica;
        u->sale = cripto::casuali(cripto::kSale);
        u->kdf = politica.nuovi;
        const Segreto kek = cripto::deriva(password, u->sale.data(), u->kdf);
        s_->scrivi_fessura(*u, kek, coppia.segreta);
        if (u->permessi.personali) s_->autentica_lista(kPersonali);
        if (u->permessi.particolari) s_->autentica_lista(kParticolari);
    }
    i.buste.erase(std::remove_if(i.buste.begin(), i.buste.end(), [&](const Busta& b) { return b.codice == codice; }),
                  i.buste.end());
}

void Cassaforte::cambia_conservazione(const std::string& data) {
    if (!kzero::day_from_text(data)) throw std::logic_error("cassaforte: data di conservazione non valida");
    if (!permessi().tutti())
        errore(Tipo::PermessoMancante, "La data di conservazione vale per tutti i dati: la puo' cambiare solo chi ha i dati "
                                       "personali e particolari (l'utente " + utente_ + " ha " + testo_permessi(permessi()) + ").");
    s_->i.conservazione = data;
}

void Cassaforte::sostituisci_contenuto(Contenuto contenuto) {
    if (!permessi().tutti())
        errore(Tipo::PermessoMancante, "Richiudere i dati li sostituisce tutti: serve chi ha i dati personali e particolari "
                                       "(l'utente " + utente_ + " ha " + testo_permessi(permessi()) + ").");
    Intestazione& i = s_->i;
    if (i.generazione == UINT32_MAX) errore(Tipo::UtentiNonAmmessi, "La cassaforte e' stata richiusa troppe volte.");
    i.generazione += 1;
    i.valori_personali = contenuto.valori.personali;
    i.valori_particolari = contenuto.valori.particolari;
    s_->voci[0] = std::move(contenuto.personali);
    s_->voci[1] = std::move(contenuto.particolari);
    s_->cifra_sezione(0);
    s_->cifra_sezione(1);
}

void Cassaforte::lega_registro(std::uint32_t voci, const Bytes& impronta) {
    if (impronta.size() != cripto::kImpronta) throw std::logic_error("cassaforte: impronta del registro non valida");
    s_->i.registro_voci = voci;
    s_->i.registro_impronta = impronta;
}

std::string Cassaforte::file() const {
    const Bytes h = s_->intestazione_autenticata();
    std::string out(h.begin(), h.end());
    for (const Bytes& c : s_->cifrate) out.append(c.begin(), c.end());
    return out;
}

}  // namespace kzero::api::cassaforte
