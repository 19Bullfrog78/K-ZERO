// K-Zero - prove interne della cassaforte (K-010): apertura, utenti e permessi, cambio delle chiavi, manomissioni.
// Argon2id con parametri bassi: solo qui, con una politica delle prove che il programma kzero non ha (cripto.hpp). Le prove
// che costruiscono file cambiati "come un altro programma" usano il formato documentato (cassaforte_formato.hpp).
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "cassaforte.hpp"
#include "cassaforte_formato.hpp"
#include "password.hpp"

using namespace kzero::api;
using cassaforte::Cassaforte;
using cassaforte::ErroreCassaforte;
using cassaforte::Permessi;
using cripto::Bytes;
using cripto::Segreto;
using Tipo = ErroreCassaforte::Tipo;

namespace {

const cripto::PoliticaKdf& politica_prove() {
    static const cripto::PoliticaKdf p{{1, 8}, {10, 2u * 1024u * 1024u}, {1, 8}};
    return p;
}

Segreto pw(const std::string& testo) {
    const Segreto scritta(reinterpret_cast<const unsigned char*>(testo.data()), testo.size());
    password::Problema problema = password::Problema::Nessuno;
    Segreto n = password::normalizza(scritta, problema);
    if (problema != password::Problema::Nessuno) throw std::logic_error("password di prova non valida");
    return n;
}

const std::string kId = "00112233445566778899aabbccddeeff";
const std::string kPwA = "cavallo pesca lampione 1";
const std::string kPwB = "barca gialla sul lago verde";
const std::string kPwC = "il treno delle sette e mezza";
const std::string kPwD = "nuvole basse sopra il monte";

Voce voce(const std::string& file, const std::string& codice, const std::string& campo, JsonDati valore, Classe classe) {
    Voce v;
    v.file = file;
    v.percorso = JsonDati::array({"persone", JsonDati{{"codice", codice}}});
    v.campo = campo;
    v.valore = std::move(valore);
    v.classe = classe;
    return v;
}

cassaforte::Contenuto contenuto() {
    cassaforte::Contenuto c;
    c.personali.push_back(voce("persone.json", "P-A", "nome", "Rossi Mario", Classe::Personale));
    c.personali.push_back(voce("persone.json", "P-B", "nome", "Bianchi Anna", Classe::Personale));
    c.particolari.push_back(voce("servizi.json", "S-1", "tipo", "disabili", Classe::Particolare));
    // Dati personali di chi va in un luogo sensibile (sezione dei legati: tutti e due i permessi).
    Voce legato = voce("persone.json", "P-L", "nome", "Bianchi Luca", Classe::Particolare);
    legato.legato = true;
    c.legati.push_back(legato);
    legato.campo = "casa";
    legato.valore = "Casa Bianchi";
    c.legati.push_back(legato);
    c.valori.personali = 2;
    c.valori.particolari = 1;
    return c;
}

Cassaforte nuova() {
    return Cassaforte::crea(kId, "2027-12-31", "A", pw(kPwA), politica_prove(), contenuto());
}

Cassaforte apri(const std::string& file, const std::string& utente, const std::string& password) {
    return Cassaforte::apri(file, kId, utente, pw(password), politica_prove());
}

Tipo tipo_errore(const std::string& file, const std::string& utente, const std::string& password) {
    try {
        apri(file, utente, password);
    } catch (const ErroreCassaforte& e) {
        return e.tipo;
    }
    ADD_FAILURE() << "la cassaforte si e' aperta";
    return Tipo::NonCassaforte;
}

// A (tutti i permessi) con B (personali), C (particolari) e D (tutti).
std::string con_quattro_utenti() {
    Cassaforte a = nuova();
    a.aggiungi_utente("B", Permessi{true, false}, pw(kPwB), politica_prove());
    a.aggiungi_utente("C", Permessi{false, true}, pw(kPwC), politica_prove());
    a.aggiungi_utente("D", Permessi{true, true}, pw(kPwD), politica_prove());
    return a.file();
}

// --- Strumenti per cambiare il file come farebbe un altro programma (docs/cassaforte.md) ------------------------------

// Le chiavi che un utente trova nella sua fessura (come le leggerebbe chi conosce il formato e la propria password).
struct Chiavi {
    Segreto segreta;
    std::array<Segreto, 3> livelli;
};

Chiavi chiavi_di(const std::string& file, const std::string& utente, const std::string& password) {
    using namespace cassaforte::formato;
    const Analizzato a = analizza(file);
    const Utente* u = a.intestazione.utente(utente);
    if (!u) throw std::logic_error("utente non trovato");
    const Segreto kek = cripto::deriva(pw(password), u->sale.data(), u->kdf);
    Segreto chiaro;
    if (!cripto::decifra(kek, u->nonce.data(), u->fessura.data(), u->fessura.size(), ad_fessura(a.intestazione.id, *u), chiaro))
        throw std::logic_error("fessura non aperta");
    Lettore r(chiaro.dati(), chiaro.dimensione());
    Chiavi k;
    const Bytes s = r.byte(cripto::kSegretaBox);
    k.segreta = Segreto(s.data(), s.size());
    const int n = r.u8();
    for (int x = 0; x < n; ++x) {
        const int l = r.u8();
        r.u32();
        const Bytes c = r.byte(cripto::kChiave);
        k.livelli[static_cast<std::size_t>(l)] = Segreto(c.data(), c.size());
    }
    return k;
}

// Riscrive il file con l'intestazione cambiata; rifa' le autenticazioni con le chiavi date (quelle che ha chi cambia il
// file): base sempre se c'e', personali e particolari se ci sono.
std::string riscrivi(const std::string& file, const std::function<void(cassaforte::formato::Intestazione&)>& cambia,
                     const std::array<const Segreto*, 3>& chiavi) {
    using namespace cassaforte::formato;
    Analizzato a = analizza(file);
    Intestazione& i = a.intestazione;
    cambia(i);
    for (int l : {kPersonali, kParticolari})
        if (chiavi[static_cast<std::size_t>(l)])
            i.autenticazione[static_cast<std::size_t>(l)] =
                cripto::autentica(derivata(*chiavi[static_cast<std::size_t>(l)], kPerAutenticazione), lista(i, l));
    Bytes h = scrivi_intestazione(i);
    if (chiavi[kBase]) {
        const Bytes prima(h.begin(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
        const Bytes t = cripto::autentica(derivata(*chiavi[kBase], kPerAutenticazione), prima);
        std::copy(t.begin(), t.end(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
    }
    return std::string(h.begin(), h.end()) + file.substr(a.fine);
}

}  // namespace

// Chiusa e riaperta: le stesse voci, i permessi del primo utente (tutti), il riepilogo senza segreti.
TEST(Cassaforte, ChiusaERiapertaUguale) {
    const std::string file = nuova().file();
    const Cassaforte c = apri(file, "A", kPwA);
    EXPECT_EQ(c.permessi(), (Permessi{true, true}));
    ASSERT_EQ(c.voci(Classe::Personale).size(), 2u);
    ASSERT_EQ(c.voci(Classe::Particolare).size(), 1u);
    EXPECT_EQ(c.voci(Classe::Personale)[0].in_json(), contenuto().personali[0].in_json());
    EXPECT_EQ(c.voci(Classe::Particolare)[0].in_json(), contenuto().particolari[0].in_json());
    const cassaforte::Riepilogo r = c.riepilogo();
    EXPECT_EQ(r.id, kId);
    EXPECT_EQ(r.generazione, 1u);
    EXPECT_EQ(r.conservazione, "2027-12-31");
    EXPECT_EQ(r.valori.personali, 2u);
    EXPECT_EQ(r.valori.particolari, 1u);
    ASSERT_EQ(r.utenti.size(), 1u);
    EXPECT_EQ(r.utenti[0].codice, "A");
    // Nell'intestazione in chiaro nessun dato: ne' i valori, ne' la password.
    for (const std::string& dato : {std::string("Rossi"), std::string("disabili"), kPwA})
        EXPECT_EQ(file.find(dato), std::string::npos) << dato;
    // Il caso: due chiusure degli stessi dati danno byte diversi.
    EXPECT_NE(file, nuova().file());
}

TEST(Cassaforte, PasswordSbagliataUtenteSconosciutoAltroPacchetto) {
    const std::string file = nuova().file();
    EXPECT_EQ(tipo_errore(file, "A", kPwB), Tipo::PasswordSbagliata);
    EXPECT_EQ(tipo_errore(file, "Z", kPwA), Tipo::UtenteSconosciuto);
    try {
        Cassaforte::apri(file, "ffeeddccbbaa99887766554433221100", "A", pw(kPwA), politica_prove());
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::AltroPacchetto);
    }
}

// La password si normalizza in NFC: "caffe'" scritto con la e accentata composta o scomposta apre la stessa fessura.
TEST(Cassaforte, LettereAccentateComposteOScomposte) {
    const std::string composta = "un caff\xC3\xA8 lungo per tutti";       // e con l'accento grave: U+00E8
    const std::string scomposta = "un caffe\xCC\x80 lungo per tutti";     // e + accento grave combinante U+0300
    const std::string file = Cassaforte::crea(kId, "2027-12-31", "A", pw(composta), politica_prove(), contenuto()).file();
    EXPECT_NO_THROW(apri(file, "A", scomposta));
    EXPECT_NO_THROW(apri(file, "A", composta));
    EXPECT_EQ(tipo_errore(file, "A", "un caffe lungo per tutti"), Tipo::PasswordSbagliata);
}

// Ogni utente legge solo le classi dei suoi permessi: senza il permesso i dati non si possono decifrare.
TEST(Cassaforte, PermessiDecidonoCosaSiDecifra) {
    const std::string file = con_quattro_utenti();
    const Cassaforte b = apri(file, "B", kPwB);
    EXPECT_EQ(b.permessi(), (Permessi{true, false}));
    EXPECT_EQ(b.voci(Classe::Personale).size(), 2u);
    EXPECT_TRUE(b.voci(Classe::Particolare).empty());
    const Cassaforte c = apri(file, "C", kPwC);
    EXPECT_TRUE(c.voci(Classe::Personale).empty());
    EXPECT_EQ(c.voci(Classe::Particolare).size(), 1u);
    // La fessura di B non contiene la chiave dei particolari (nemmeno cifrata per lui).
    const Chiavi kb = chiavi_di(file, "B", kPwB);
    EXPECT_TRUE(kb.livelli[2].vuoto());
    EXPECT_FALSE(kb.livelli[1].vuoto());
}

// Si danno al massimo i permessi che si hanno; non si toglie chi ha permessi che non si hanno.
TEST(Cassaforte, AlMassimoIPropriPermessi) {
    Cassaforte b = apri(con_quattro_utenti(), "B", kPwB);
    try {
        b.aggiungi_utente("E", Permessi{true, true}, pw(kPwD), politica_prove());
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::PermessoMancante);
    }
    try {
        b.togli_utente("D");
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::PermessoMancante);
    }
    try {
        b.cambia_password("C", pw(kPwD), politica_prove());
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::PermessoMancante);
    }
    try {
        b.cambia_conservazione("2030-01-01");
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::PermessoMancante);
    }
    EXPECT_NO_THROW(b.aggiungi_utente("E", Permessi{true, false}, pw(kPwD), politica_prove()));
    const Cassaforte e = apri(b.file(), "E", kPwD);
    EXPECT_EQ(e.voci(Classe::Personale).size(), 2u);
}

TEST(Cassaforte, UltimoUtenteEUltimoLettore) {
    Cassaforte a = nuova();
    try {
        a.togli_utente("A");
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::UtentiNonAmmessi);
    }
    a.aggiungi_utente("B", Permessi{true, false}, pw(kPwB), politica_prove());
    try {
        a.togli_utente("A");   // ultimo che legge i particolari
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::UtentiNonAmmessi);
    }
    try {
        a.aggiungi_utente("B", Permessi{true, false}, pw(kPwC), politica_prove());
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::UtentiNonAmmessi);
    }
}

// Togliere un utente: chiavi nuove; l'utente tolto non apre la versione nuova, gli altri si (con le buste).
TEST(Cassaforte, UtenteToltoNonApreLaVersioneNuova) {
    const std::string prima = con_quattro_utenti();
    Cassaforte d = apri(prima, "D", kPwD);
    d.togli_utente("B");
    const std::string dopo = d.file();
    EXPECT_EQ(tipo_errore(dopo, "B", kPwB), Tipo::UtenteSconosciuto);
    // Anche rimettendo a mano la vecchia voce di B nell'intestazione: le sue chiavi sono quelle vecchie.
    const Cassaforte a = apri(dopo, "A", kPwA);   // A riceve base e personali nuove in busta
    EXPECT_EQ(a.voci(Classe::Personale).size(), 2u);
    const Cassaforte c = apri(dopo, "C", kPwC);   // C riceve solo la base nuova
    EXPECT_EQ(c.voci(Classe::Particolare).size(), 1u);
    const Chiavi vecchie_b = chiavi_di(prima, "B", kPwB);
    const Chiavi nuove_a = chiavi_di(dopo, "A", kPwA);   // fessura di A ancora vecchia: le nuove le ha in busta
    const Cassaforte d2 = apri(dopo, "D", kPwD);
    EXPECT_EQ(d2.versione_base(), 2u);
    // La copia vecchia resta leggibile con le chiavi vecchie (rischio scritto in docs/sicurezza.md).
    EXPECT_NO_THROW(apri(prima, "B", kPwB));
    // Un secondo cambio delle chiavi: A, rimasto indietro di due versioni, apre ancora (storia delle chiavi).
    Cassaforte d3 = apri(dopo, "D", kPwD);
    d3.aggiungi_utente("E", Permessi{true, false}, pw(kPwB), politica_prove());
    const std::string con_e = d3.file();
    Cassaforte d4 = apri(con_e, "D", kPwD);
    d4.togli_utente("E");
    const std::string terza = d4.file();
    const Cassaforte a3 = apri(terza, "A", kPwA);
    EXPECT_EQ(a3.versione_base(), 3u);
    EXPECT_EQ(a3.voci(Classe::Personale).size(), 2u);
    EXPECT_NO_THROW(a3.chiave_registro(1));
    EXPECT_NO_THROW(a3.chiave_registro(2));
    EXPECT_NO_THROW(a3.chiave_registro(3));
    // Le chiavi che B aveva (le legge dalla sua copia vecchia) non decifrano la sezione nuova dei dati personali.
    {
        using namespace cassaforte::formato;
        const Analizzato x = analizza(dopo);
        const Sezione& sez = x.intestazione.sezioni[0];
        const std::string cifrata = dopo.substr(x.fine, static_cast<std::size_t>(sez.lunghezza));
        Segreto testo;
        EXPECT_FALSE(cripto::decifra(derivata(vecchie_b.livelli[kPersonali], kPerSezione), sez.nonce.data(),
                                     reinterpret_cast<const unsigned char*>(cifrata.data()), cifrata.size(),
                                     ad_sezione(x.intestazione.id, kPersonali, x.intestazione.versioni[kPersonali],
                                                x.intestazione.generazione),
                                     testo));
    }
    // B rimette a mano la sua vecchia voce nell'intestazione e rifa' le autenticazioni con le chiavi che aveva: non apre, e
    // chi e' rimasto se ne accorge.
    {
        using namespace cassaforte::formato;
        const Analizzato vecchio = analizza(prima);
        const Utente b = *vecchio.intestazione.utente("B");
        const std::string con_b = riscrivi(dopo, [&](Intestazione& i) {
            i.utenti.push_back(b);
            std::sort(i.utenti.begin(), i.utenti.end(), [](const Utente& x, const Utente& y) { return x.codice < y.codice; });
        }, {&vecchie_b.livelli[kBase], &vecchie_b.livelli[kPersonali], nullptr});
        tipo_errore(con_b, "B", kPwB);
        EXPECT_EQ(tipo_errore(con_b, "A", kPwA), Tipo::Manomessa);
        EXPECT_EQ(tipo_errore(con_b, "D", kPwD), Tipo::Manomessa);
    }
    (void)nuove_a;
}

TEST(Cassaforte, PasswordNuovaPerSeEPerUnAltro) {
    Cassaforte a = apri(con_quattro_utenti(), "A", kPwA);
    a.cambia_password("A", pw("una password nuova di zecca"), politica_prove());
    a.cambia_password("B", pw("anche la sua e' nuova adesso"), politica_prove());
    const std::string file = a.file();
    EXPECT_EQ(tipo_errore(file, "A", kPwA), Tipo::PasswordSbagliata);
    EXPECT_NO_THROW(apri(file, "A", "una password nuova di zecca"));
    EXPECT_EQ(tipo_errore(file, "B", kPwB), Tipo::PasswordSbagliata);
    const Cassaforte b = apri(file, "B", "anche la sua e' nuova adesso");
    EXPECT_EQ(b.voci(Classe::Personale).size(), 2u);
    EXPECT_NO_THROW(apri(file, "C", kPwC));
}

TEST(Cassaforte, RichiusuraEConservazione) {
    Cassaforte a = nuova();
    cassaforte::Contenuto c = contenuto();
    c.personali.pop_back();
    c.valori.personali = 1;
    a.sostituisci_contenuto(c);
    a.cambia_conservazione("2030-06-30");
    a.lega_registro(7, Bytes(cripto::kImpronta, 0xAB));
    const Cassaforte r = apri(a.file(), "A", kPwA);
    EXPECT_EQ(r.voci(Classe::Personale).size(), 1u);
    EXPECT_EQ(r.riepilogo().generazione, 2u);
    EXPECT_EQ(r.riepilogo().conservazione, "2030-06-30");
    EXPECT_EQ(r.riepilogo().registro_voci, 7u);
}

// --- Manomissioni ---------------------------------------------------------------------------------------------------

// Un byte cambiato in qualunque punto dell'intestazione, in ogni sezione: mai aperta, sempre un errore chiaro.
TEST(Cassaforte, UnByteCambiatoInOgniPunto) {
    const std::string file = con_quattro_utenti();
    const std::size_t fine = cassaforte::formato::analizza(file).fine;
    std::set<int> tipi;
    for (std::size_t k = 0; k < file.size(); k += (k < fine ? 1 : 7)) {
        for (const std::string& utente : {std::string("A"), std::string("C")}) {
            std::string x = file;
            x[k] = static_cast<char>(x[k] ^ 0x01);
            try {
                apri(x, utente, utente == "A" ? kPwA : kPwC);
                // Un byte che non e' letto da nessuno non puo' esserci: ogni byte e' autenticato.
                ADD_FAILURE() << "aperta con il byte " << k << " cambiato (utente " << utente << ")";
            } catch (const ErroreCassaforte& e) {
                tipi.insert(static_cast<int>(e.tipo));
            }
        }
    }
    EXPECT_TRUE(tipi.count(static_cast<int>(Tipo::Manomessa)));
    EXPECT_TRUE(tipi.count(static_cast<int>(Tipo::NonCassaforte)));
}

TEST(Cassaforte, TroncataVuotaAltroFile) {
    const std::string file = nuova().file();
    for (std::size_t n = 0; n < file.size(); n += (n < 600 ? 1 : 97)) {
        try {
            apri(file.substr(0, n), "A", kPwA);
            ADD_FAILURE() << "aperta troncata a " << n;
        } catch (const ErroreCassaforte& e) {
            EXPECT_EQ(e.tipo, Tipo::NonCassaforte) << n;
        }
    }
    EXPECT_EQ(tipo_errore(file + "x", "A", kPwA), Tipo::NonCassaforte);
    EXPECT_EQ(tipo_errore("{\"schema\": \"pacchetto\"}", "A", kPwA), Tipo::NonCassaforte);
    std::string versione = file;
    versione[8] = 2;
    EXPECT_EQ(tipo_errore(versione, "A", kPwA), Tipo::VersioneSconosciuta);
}

// Parametri di Argon2id abbassati o assurdi: rifiutati prima di calcolare (con la politica del programma vero).
TEST(Cassaforte, ParametriAbbassatiOAssurdi) {
    const std::string file = nuova().file();   // parametri bassi delle prove
    try {
        Cassaforte::apri(file, kId, "A", pw(kPwA), cripto::politica_di_rilascio());
        ADD_FAILURE();
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::ParametriNonAmmessi);
    }
    const std::string assurdi = riscrivi(file, [](cassaforte::formato::Intestazione& i) {
        i.utenti[0].kdf.memoria_kib = 0xFFFFFFFFu;   // 4 TiB
        i.utenti[0].kdf.operazioni = 0xFFFFFFFFu;
    }, {nullptr, nullptr, nullptr});
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(tipo_errore(assurdi, "A", kPwA), Tipo::ParametriNonAmmessi);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(2));
}

// Arbitro, risposta 1 f: la chiave pubblica di un utente sostituita nel file e' rifiutata prima del cambio delle chiavi.
TEST(Cassaforte, ChiavePubblicaSostituitaRifiutata) {
    const std::string file = con_quattro_utenti();
    const cripto::CoppiaBox finta = cripto::nuova_coppia_box();
    // Da fuori (senza nessuna chiave): l'autenticazione dell'intestazione non torna.
    const std::string da_fuori = riscrivi(file, [&](cassaforte::formato::Intestazione& i) {
        i.utente("C")->pubblica = finta.pubblica;
    }, {nullptr, nullptr, nullptr});
    for (const auto& [u, p] : {std::pair{"A", kPwA}, std::pair{"D", kPwD}, std::pair{"B", kPwB}})
        EXPECT_EQ(tipo_errore(da_fuori, u, p), Tipo::Manomessa) << u;
    // Da un utente con i soli dati personali (B), che rifa' base e personali con le sue chiavi: chi ha i particolari se ne
    // accorge (elenco dei particolari autenticato con la chiave dei particolari) e non toglie nessuno.
    const Chiavi kb = chiavi_di(file, "B", kPwB);
    const std::string da_b = riscrivi(file, [&](cassaforte::formato::Intestazione& i) {
        i.utente("C")->pubblica = finta.pubblica;
    }, {&kb.livelli[0], &kb.livelli[1], nullptr});
    EXPECT_EQ(tipo_errore(da_b, "D", kPwD), Tipo::Manomessa);
    EXPECT_EQ(tipo_errore(da_b, "A", kPwA), Tipo::Manomessa);
}

// Arbitro, risposta 1 f: un utente aggiunto da chi non ha una password (nessuna chiave) e' rifiutato.
TEST(Cassaforte, UtenteAggiuntoDaFuoriRifiutato) {
    const std::string file = nuova().file();
    // Da fuori si puo' solo copiare una fessura vera sotto un altro nome, o farne una con chiavi inventate.
    const std::string da_fuori = riscrivi(file, [](cassaforte::formato::Intestazione& i) {
        cassaforte::formato::Utente x = i.utenti[0];
        x.codice = "INTRUSO";
        i.utenti.push_back(x);
    }, {nullptr, nullptr, nullptr});
    EXPECT_EQ(tipo_errore(da_fuori, "A", kPwA), Tipo::Manomessa);
    EXPECT_NE(tipo_errore(da_fuori, "INTRUSO", kPwA), Tipo::Manomessa);   // la sua fessura non si apre nemmeno
}

// Arbitro, risposta 1 f: permesso dei particolari alzato a mano da un utente con i soli dati personali, che rifa'
// l'autenticazione con i suoi segreti: rifiutato da chi ha i particolari, e nessun cambio di chiavi gli consegna la chiave.
TEST(Cassaforte, PermessoAlzatoAManoRifiutato) {
    const std::string file = con_quattro_utenti();
    const Chiavi kb = chiavi_di(file, "B", kPwB);
    const std::string alzato = riscrivi(file, [](cassaforte::formato::Intestazione& i) {
        cassaforte::formato::Utente* b = i.utente("B");
        b->permessi.particolari = true;
        // La fessura deve avere la lunghezza giusta: B la puo' rifare (conosce la sua password), ma senza la chiave dei
        // particolari, che non ha.
        b->fessura.resize(cassaforte::formato::lunghezza_fessura(b->permessi));
    }, {&kb.livelli[0], &kb.livelli[1], nullptr});
    // Chi ha i particolari: l'elenco dei particolari non e' autentico.
    EXPECT_EQ(tipo_errore(alzato, "D", kPwD), Tipo::Manomessa);
    EXPECT_EQ(tipo_errore(alzato, "C", kPwC), Tipo::Manomessa);
    // B stesso non apre (la sua fessura non corrisponde ai permessi scritti).
    EXPECT_NE(tipo_errore(alzato, "B", kPwB), static_cast<Tipo>(-1));
    // Un cambio delle chiavi non consegna mai a B la chiave dei particolari: chi ha i soli personali (E) non puo' farlo, chi
    // puo' farlo se ne accorge.
    Cassaforte a = apri(file, "A", kPwA);
    a.aggiungi_utente("E", Permessi{true, false}, pw(kPwD), politica_prove());
    a.aggiungi_utente("F", Permessi{true, false}, pw(kPwC), politica_prove());
    const std::string con_e = a.file();
    const Chiavi kb2 = chiavi_di(con_e, "B", kPwB);
    const std::string alzato2 = riscrivi(con_e, [](cassaforte::formato::Intestazione& i) {
        cassaforte::formato::Utente* b = i.utente("B");
        b->permessi.particolari = true;
        b->fessura.resize(cassaforte::formato::lunghezza_fessura(b->permessi));
    }, {&kb2.livelli[0], &kb2.livelli[1], nullptr});
    // E apre (non controlla l'elenco dei particolari, non ne ha la chiave) ma non puo' rifare le chiavi: dall'aggiunta 1 togliere
    // un utente lo fa solo chi ha tutti e due i permessi (i legati si ricifrano con le due chiavi di classe).
    Cassaforte e = apri(alzato2, "E", kPwD);
    try {
        e.togli_utente("F");
        ADD_FAILURE() << "un utente con i soli dati personali ha rifatto le chiavi";
    } catch (const ErroreCassaforte& x) {
        EXPECT_EQ(x.tipo, Tipo::PermessoMancante);
    }
    // Chi puo' rifare le chiavi ha i particolari: controlla l'elenco e rifiuta il file prima di consegnare qualunque chiave.
    EXPECT_EQ(tipo_errore(alzato2, "D", kPwD), Tipo::Manomessa);
    EXPECT_EQ(tipo_errore(alzato2, "A", kPwA), Tipo::Manomessa);
    // Nel file alzato non c'e' nessuna busta dei particolari per B.
    for (const cassaforte::formato::Busta& b : cassaforte::formato::analizza(alzato2).intestazione.buste)
        EXPECT_FALSE(b.codice == "B" && b.livello == cassaforte::formato::kParticolari);
}

// Una busta preparata da chi non aveva la chiave vecchia (es. da fuori, con la chiave pubblica che e' in chiaro) non vale.
TEST(Cassaforte, BustaFintaNonVale) {
    const std::string prima = con_quattro_utenti();
    Cassaforte d = apri(prima, "D", kPwD);
    d.togli_utente("B");
    const std::string dopo = d.file();
    // Sostituisce la busta dei personali di A con una che contiene una chiave inventata (stessa versione).
    const std::string finto = riscrivi(dopo, [](cassaforte::formato::Intestazione& i) {
        for (cassaforte::formato::Busta& b : i.buste) {
            if (b.codice != "A" || b.livello != cassaforte::formato::kPersonali) continue;
            const Bytes k = cripto::casuali(cripto::kChiave);
            const Segreto chiave(k.data(), k.size());
            const Bytes dentro = cassaforte::formato::contenuto_busta(i.id, b.livello, b.versione, chiave);
            b.busta = cripto::sigilla(dentro.data(), dentro.size(), i.utente("A")->pubblica);
        }
    }, {nullptr, nullptr, nullptr});
    EXPECT_EQ(tipo_errore(finto, "A", kPwA), Tipo::Manomessa);
}

// Una versione delle chiavi abbassata nell'intestazione si scopre.
TEST(Cassaforte, VersioneDelleChiaviAbbassata) {
    Cassaforte d = apri(con_quattro_utenti(), "D", kPwD);
    d.togli_utente("B");
    const std::string dopo = d.file();
    const Chiavi kd = chiavi_di(dopo, "D", kPwD);
    const std::string giu = riscrivi(dopo, [](cassaforte::formato::Intestazione& i) {
        i.versioni[0] = 1;
        i.storie[0].clear();
    }, {&kd.livelli[0], nullptr, nullptr});
    EXPECT_EQ(tipo_errore(giu, "D", kPwD), Tipo::Manomessa);
}

namespace {

// Tutte le chiavi che un utente si puo' procurare dal file con la sua password, come farebbe un altro programma: quelle
// della sua fessura e quelle delle buste per lui (le chiavi vecchie della storia, con una cassaforte appena fatta o con un
// solo cambio, sono gia' tra queste).
std::vector<Segreto> chiavi_procurabili(const std::string& file, const std::string& utente, const std::string& password) {
    using namespace cassaforte::formato;
    Chiavi k = chiavi_di(file, utente, password);
    std::vector<Segreto> out;
    for (Segreto& x : k.livelli)
        if (x.dimensione() == cripto::kChiave) out.push_back(std::move(x));
    const Analizzato a = analizza(file);
    const Utente* u = a.intestazione.utente(utente);
    for (const Busta& b : a.intestazione.buste) {
        if (b.codice != utente) continue;
        Segreto dentro;
        if (!cripto::apri_busta(b.busta, u->pubblica, k.segreta, dentro)) continue;
        Lettore r(dentro.dati(), dentro.dimensione());
        r.byte(cassaforte::kId);
        r.u8();
        r.u32();
        const Bytes chiave = r.byte(cripto::kChiave);
        if (r.ok()) out.emplace_back(chiave.data(), chiave.size());
    }
    return out;
}

// I testi in chiaro di tutte le sezioni che si aprono con le chiavi date: ogni chiave cosi' com'e', ogni sua chiave
// derivata (usi da 1 a 5), la chiave dei legati calcolata con ogni coppia delle chiavi date e con una chiave inventata al
// posto di quella che manca; ogni dato associato del formato.
std::vector<std::string> testi_decifrabili(const std::string& file, const std::vector<Segreto>& chiavi) {
    using namespace cassaforte::formato;
    const Analizzato a = analizza(file);
    const Intestazione& i = a.intestazione;
    std::vector<Segreto> candidate;
    const Bytes inventata = cripto::casuali(cripto::kChiave);
    const Segreto altra(inventata.data(), inventata.size());
    for (const Segreto& k : chiavi) {
        candidate.emplace_back(k.dati(), k.dimensione());
        for (std::uint64_t uso = 1; uso <= 5; ++uso) candidate.push_back(derivata(k, uso));
        candidate.push_back(chiave_legati(i.id, k, altra));
        candidate.push_back(chiave_legati(i.id, altra, k));
        for (const Segreto& h : chiavi) candidate.push_back(chiave_legati(i.id, k, h));
    }
    const std::vector<Bytes> associati = {
        ad_legati(i.id, i.versioni[kPersonali], i.versioni[kParticolari], i.generazione),
        ad_sezione(i.id, kPersonali, i.versioni[kPersonali], i.generazione),
        ad_sezione(i.id, kParticolari, i.versioni[kParticolari], i.generazione)};
    std::vector<std::string> out;
    std::size_t inizio = a.fine;
    for (const Sezione& sez : i.sezioni) {
        const auto* p = reinterpret_cast<const unsigned char*>(file.data()) + inizio;
        inizio += static_cast<std::size_t>(sez.lunghezza);
        for (const Segreto& k : candidate)
            for (const Bytes& ad : associati) {
                Segreto testo;
                if (cripto::decifra(k, sez.nonce.data(), p, static_cast<std::size_t>(sez.lunghezza), ad, testo))
                    out.emplace_back(testo.testo());
            }
    }
    return out;
}

// Nessun testo decifrabile con le chiavi di quell'utente contiene i dati legati del contenuto delle prove.
void nessun_legato(const std::string& file, const std::string& utente, const std::string& password) {
    for (const std::string& t : testi_decifrabili(file, chiavi_procurabili(file, utente, password)))
        for (const char* legato : {"Bianchi Luca", "Casa Bianchi"})
            EXPECT_EQ(t.find(legato), std::string::npos) << utente << " decifra " << legato;
}

}  // namespace

// Aggiunta 1 del K-010, punto 1: i dati personali di chi e' legato a un luogo sensibile si decifrano solo con tutti e due i
// permessi. Prova sul file, non solo sull'uscita: chi ha i soli personali o i soli particolari prova tutte le chiavi che si
// puo' procurare, su tutte le sezioni, e non legge mai quei dati; chi ha tutti e due si'. Lo stesso dopo un cambio delle
// chiavi dei personali e dopo uno dei particolari.
TEST(Cassaforte, LegatiSoloConTuttiEDueIPermessi) {
    const std::string file = con_quattro_utenti();   // A e D tutti i permessi, B i soli personali, C i soli particolari
    for (const auto& [utente, password, legati] :
         {std::tuple{"A", kPwA, 2u}, std::tuple{"B", kPwB, 0u}, std::tuple{"C", kPwC, 0u}, std::tuple{"D", kPwD, 2u}})
        EXPECT_EQ(apri(file, utente, password).voci_legate().size(), legati) << utente;
    nessun_legato(file, "B", kPwB);
    nessun_legato(file, "C", kPwC);
    // Il metodo funziona: con le stesse chiavi B legge la sua sezione, A anche i legati.
    bool personali_di_b = false;
    for (const std::string& t : testi_decifrabili(file, chiavi_procurabili(file, "B", kPwB)))
        personali_di_b = personali_di_b || t.find("Rossi Mario") != std::string::npos;
    EXPECT_TRUE(personali_di_b);
    bool legati_di_a = false;
    for (const std::string& t : testi_decifrabili(file, chiavi_procurabili(file, "A", kPwA)))
        legati_di_a = legati_di_a || t.find("Bianchi Luca") != std::string::npos;
    EXPECT_TRUE(legati_di_a);

    // Un cambio delle chiavi dei personali (D toglie un altro utente con i soli personali): i legati si ricifrano.
    Cassaforte d = apri(file, "D", kPwD);
    d.aggiungi_utente("E", Permessi{true, false}, pw("un utente in piu' con i personali"), politica_prove());
    d.togli_utente("E");
    const std::string dopo_personali = d.file();
    EXPECT_NE(cassaforte::formato::analizza(dopo_personali).intestazione.sezioni[cassaforte::formato::kSezioneLegati].impronta,
              cassaforte::formato::analizza(file).intestazione.sezioni[cassaforte::formato::kSezioneLegati].impronta);
    EXPECT_EQ(apri(dopo_personali, "A", kPwA).voci_legate().size(), 2u);   // A: chiavi nuove in busta
    EXPECT_TRUE(apri(dopo_personali, "B", kPwB).voci_legate().empty());
    EXPECT_TRUE(apri(dopo_personali, "C", kPwC).voci_legate().empty());
    nessun_legato(dopo_personali, "B", kPwB);
    nessun_legato(dopo_personali, "C", kPwC);

    // Un cambio delle chiavi dei particolari (D toglie C).
    Cassaforte d2 = apri(dopo_personali, "D", kPwD);
    d2.togli_utente("C");
    const std::string dopo_particolari = d2.file();
    EXPECT_EQ(apri(dopo_particolari, "A", kPwA).voci_legate().size(), 2u);
    EXPECT_EQ(apri(dopo_particolari, "D", kPwD).voci_legate().size(), 2u);
    EXPECT_TRUE(apri(dopo_particolari, "B", kPwB).voci_legate().empty());
    nessun_legato(dopo_particolari, "B", kPwB);
    // C, tolto, con la sua copia vecchia non legge comunque i legati (non li ha mai potuti leggere).
    nessun_legato(dopo_personali, "C", kPwC);

    // Togliere un utente rifa' le chiavi e ricifra i legati: serve chi ha tutti e due i permessi.
    Cassaforte b = apri(dopo_particolari, "B", kPwB);
    b.aggiungi_utente("F", Permessi{true, false}, pw("un altro utente con i personali"), politica_prove());
    try {
        b.togli_utente("F");
        ADD_FAILURE() << "un utente con i soli dati personali ha rifatto le chiavi";
    } catch (const ErroreCassaforte& e) {
        EXPECT_EQ(e.tipo, Tipo::PermessoMancante);
    }
}

namespace {

// "file campo" -> quante voci, mai i valori.
using Elenco = std::map<std::string, std::size_t>;
Elenco elenco_di(const std::vector<Voce>& voci) {
    Elenco e;
    for (const Voce& v : voci) ++e[v.file + " " + v.campo];
    return e;
}

std::string testo_elenco(const Elenco& e) {
    std::string t;
    for (const auto& [chiave, n] : e) t += "      " + chiave + ": " + std::to_string(n) + "\n";
    return t.empty() ? "      (nessuna)\n" : t;
}

std::string leggi_file(const std::string& percorso) {
    std::ifstream in(percorso, std::ios::binary);
    if (!in) throw std::logic_error("file non trovato: " + percorso);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// I testi in chiaro delle tre sezioni, decifrati come farebbe un altro programma con le chiavi della fessura di un utente con
// tutti e due i permessi (la copia congelata non ha mai cambiato chiavi: quelle della fessura sono le chiavi di adesso).
std::array<std::string, 3> sezioni_in_chiaro(const std::string& file, const std::string& utente, const std::string& password) {
    using namespace cassaforte::formato;
    const Chiavi k = chiavi_di(file, utente, password);
    const Analizzato a = analizza(file);
    const Intestazione& i = a.intestazione;
    const Segreto& kp = k.livelli[kPersonali];
    const Segreto& kq = k.livelli[kParticolari];
    const std::array<Segreto, 3> chiavi{derivata(kp, kPerSezione), derivata(kq, kPerSezione), chiave_legati(i.id, kp, kq)};
    const std::array<Bytes, 3> associati{ad_sezione(i.id, kPersonali, i.versioni[kPersonali], i.generazione),
                                         ad_sezione(i.id, kParticolari, i.versioni[kParticolari], i.generazione),
                                         ad_legati(i.id, i.versioni[kPersonali], i.versioni[kParticolari], i.generazione)};
    std::array<std::string, 3> out;
    std::size_t inizio = a.fine;
    for (std::size_t x = 0; x < kSezioni; ++x) {
        const Sezione& sez = i.sezioni[x];
        Segreto testo;
        if (!cripto::decifra(chiavi[x], sez.nonce.data(), reinterpret_cast<const unsigned char*>(file.data()) + inizio,
                             static_cast<std::size_t>(sez.lunghezza), associati[x], testo))
            throw std::logic_error("sezione non decifrata con le chiavi di " + utente);
        out[x] = std::string(testo.testo());
        inizio += static_cast<std::size_t>(sez.lunghezza);
    }
    return out;
}

}  // namespace

// Aggiunta 1 del K-010, punto 1, sul demo: la copia congelata (tests/esempi/k010-cassaforte, chiusa con il programma vero e i
// parametri di rilascio) aperta dai suoi tre utenti. Per ognuno l'elenco delle voci che decifra (file e campo, mai i valori),
// stampato per il resoconto. Chi ha i soli personali o i soli particolari non decifra le voci legate a un luogo sensibile:
// ne' aprendo la cassaforte, ne' provando sul file tutte le chiavi che si procura.
TEST(Cassaforte, CopiaCongelataDelDemoVociPerUtente) {
    const std::string cartella = std::string(KZERO_CARTELLA_PROVE) + "/esempi/k010-cassaforte/";
    const std::string file = leggi_file(cartella + cassaforte::kFile);
    const std::string id = cassaforte::leggi_intestazione(file).id;
    struct Utente {
        const char* codice;
        const char* password;
        Permessi permessi;
    };
    const Utente titolare{"TITOLARE", "congelata per sempre nelle prove", {true, true}};
    const Utente ufficio{"UFFICIO", "ufficio legge solo i dati personali", {true, false}};
    const Utente salute{"SALUTE", "salute legge solo i dati particolari", {false, true}};

    const Cassaforte tutto = Cassaforte::apri(file, id, titolare.codice, pw(titolare.password), cripto::politica_di_rilascio());
    const Elenco personali = elenco_di(tutto.voci(Classe::Personale));
    const Elenco particolari = elenco_di(tutto.voci(Classe::Particolare));
    const Elenco legati = elenco_di(tutto.voci_legate());
    ASSERT_FALSE(legati.empty());   // il demo ha passeggeri che vanno al centro diurno
    for (const Voce& v : tutto.voci_legate()) EXPECT_TRUE(v.legato && v.classe == Classe::Particolare) << v.file << " " << v.campo;

    std::cout << "Voci decifrate (file campo: quante), copia congelata del demo:\n";
    for (const Utente& u : {titolare, ufficio, salute}) {
        const Cassaforte c = Cassaforte::apri(file, id, u.codice, pw(u.password), cripto::politica_di_rilascio());
        EXPECT_EQ(c.permessi(), u.permessi);
        const Elenco p = elenco_di(c.voci(Classe::Personale));
        const Elenco q = elenco_di(c.voci(Classe::Particolare));
        const Elenco l = elenco_di(c.voci_legate());
        EXPECT_EQ(p, u.permessi.personali ? personali : Elenco{}) << u.codice;
        EXPECT_EQ(q, u.permessi.particolari ? particolari : Elenco{}) << u.codice;
        EXPECT_EQ(l, u.permessi.tutti() ? legati : Elenco{}) << u.codice;
        std::cout << "  " << u.codice << " (" << cassaforte::testo_permessi_breve(u.permessi) << ")\n"
                  << "    sezione dei personali:\n" << testo_elenco(p)
                  << "    sezione dei particolari:\n" << testo_elenco(q)
                  << "    sezione dei legati (personali + particolari):\n" << testo_elenco(l);
    }

    // Sul file: i testi in chiaro delle tre sezioni (con le chiavi di TITOLARE), poi tutto cio' che UFFICIO e SALUTE riescono
    // a decifrare con le chiavi che si procurano. Nessuno dei due ottiene la sezione dei legati, ne' uno dei suoi valori che
    // non sta anche in un'altra sezione; ognuno ottiene la sua.
    const std::array<std::string, 3> chiaro = sezioni_in_chiaro(file, titolare.codice, titolare.password);
    std::vector<std::string> solo_legati;
    for (const Voce& v : tutto.voci_legate()) {
        if (!v.valore.is_string()) continue;
        const std::string t = v.valore.get<std::string>();
        if (t.size() < 4 || t.find_first_of("\"\\") != std::string::npos) continue;
        if (chiaro[cassaforte::formato::kSezionePersonali].find(t) == std::string::npos &&
            chiaro[cassaforte::formato::kSezioneParticolari].find(t) == std::string::npos)
            solo_legati.push_back(t);
    }
    ASSERT_FALSE(solo_legati.empty());
    for (const auto& [u, sua] : {std::pair{ufficio, cassaforte::formato::kSezionePersonali},
                                 std::pair{salute, cassaforte::formato::kSezioneParticolari}}) {
        const std::vector<std::string> testi = testi_decifrabili(file, chiavi_procurabili(file, u.codice, u.password));
        EXPECT_NE(std::find(testi.begin(), testi.end(), chiaro[sua]), testi.end()) << u.codice;
        EXPECT_EQ(std::find(testi.begin(), testi.end(), chiaro[cassaforte::formato::kSezioneLegati]), testi.end())
            << u.codice << " decifra la sezione dei legati";
        std::size_t trovati = 0;
        for (const std::string& t : testi)
            for (const std::string& valore : solo_legati) trovati += t.find(valore) != std::string::npos;
        EXPECT_EQ(trovati, 0u) << u.codice << " decifra dati legati";
    }
    std::cout << "  Sul file: UFFICIO e SALUTE non decifrano la sezione dei legati (" << solo_legati.size()
              << " valori controllati, che non stanno in altre sezioni).\n";
}
