// K-Zero - prove interne della cassaforte (K-010): apertura, utenti e permessi, cambio delle chiavi, manomissioni.
// Argon2id con parametri bassi: solo qui, con una politica delle prove che il programma kzero non ha (cripto.hpp). Le prove
// che costruiscono file cambiati "come un altro programma" usano il formato documentato (cassaforte_formato.hpp).
#include <algorithm>
#include <chrono>
#include <functional>
#include <set>
#include <string>
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
    // Un utente con i soli personali (E) che toglie qualcuno non puo' controllare l'elenco dei particolari, ma non ha la
    // chiave dei particolari: dopo il cambio delle chiavi nessuna busta dei particolari va a B.
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
    Cassaforte e = apri(alzato2, "E", kPwD);
    e.togli_utente("F");
    const std::string dopo = e.file();
    const cassaforte::formato::Analizzato x = cassaforte::formato::analizza(dopo);
    for (const cassaforte::formato::Busta& b : x.intestazione.buste)
        EXPECT_FALSE(b.codice == "B" && b.livello == cassaforte::formato::kParticolari);
    EXPECT_EQ(tipo_errore(dopo, "D", kPwD), Tipo::Manomessa);   // e chi ha i particolari se ne accorge ancora
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
