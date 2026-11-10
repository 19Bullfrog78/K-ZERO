// Prove della cifratura (libsodium) e delle password (K-010).
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "cripto.hpp"
#include "password.hpp"
#include "unicode.hpp"

using namespace kzero::api;
using cripto::Bytes;
using cripto::Segreto;

namespace {

Segreto segreto(const std::string& s) { return Segreto(reinterpret_cast<const unsigned char*>(s.data()), s.size()); }

// Parametri bassi: solo nelle prove (il programma usa sempre la politica di rilascio).
const cripto::PoliticaKdf kProve{{1, 8}, {10, 2u * 1024u * 1024u}, {1, 8}};

}  // namespace

TEST(Cripto, CifraturaAutenticataScopreOgniCambiamento) {
    const Segreto k(cripto::casuali(cripto::kChiave).data(), cripto::kChiave);
    const Bytes nonce = cripto::casuali(cripto::kNonce);
    const std::string testo = "Bianchi Luca";
    const Bytes ad = {1, 2, 3};
    const Bytes c = cripto::cifra(k, nonce.data(), reinterpret_cast<const unsigned char*>(testo.data()), testo.size(), ad);
    ASSERT_EQ(c.size(), testo.size() + cripto::kEtichetta);
    Segreto chiaro;
    ASSERT_TRUE(cripto::decifra(k, nonce.data(), c.data(), c.size(), ad, chiaro));
    EXPECT_EQ(chiaro.testo(), testo);
    for (std::size_t i = 0; i < c.size(); ++i) {   // un byte cambiato in ogni punto
        Bytes x = c;
        x[i] ^= 0x01;
        Segreto y;
        EXPECT_FALSE(cripto::decifra(k, nonce.data(), x.data(), x.size(), ad, y)) << i;
    }
    Bytes n2 = nonce;
    n2[0] ^= 1;
    EXPECT_FALSE(cripto::decifra(k, n2.data(), c.data(), c.size(), ad, chiaro));
    EXPECT_FALSE(cripto::decifra(k, nonce.data(), c.data(), c.size(), Bytes{1, 2, 4}, chiaro));
    const Segreto altra(cripto::casuali(cripto::kChiave).data(), cripto::kChiave);
    EXPECT_FALSE(cripto::decifra(altra, nonce.data(), c.data(), c.size(), ad, chiaro));
    EXPECT_FALSE(cripto::decifra(k, nonce.data(), c.data(), 5, ad, chiaro));   // troncato
}

TEST(Cripto, Argon2idStessaPasswordStessaChiave) {
    const Segreto pw = segreto("una frase lunga per la prova");
    const Bytes sale = cripto::casuali(cripto::kSale);
    const cripto::ParametriKdf p{1, 64};
    const Segreto a = cripto::deriva(pw, sale.data(), p);
    const Segreto b = cripto::deriva(pw, sale.data(), p);
    EXPECT_TRUE(cripto::uguali(a.dati(), b.dati(), cripto::kChiave));
    Bytes sale2 = sale;
    sale2[0] ^= 1;
    const Segreto c = cripto::deriva(pw, sale2.data(), p);
    EXPECT_FALSE(cripto::uguali(a.dati(), c.dati(), cripto::kChiave));
    const Segreto d = cripto::deriva(pw, sale.data(), cripto::ParametriKdf{2, 64});
    EXPECT_FALSE(cripto::uguali(a.dati(), d.dati(), cripto::kChiave));
}

TEST(Cripto, ParametriControllatiPrimaDiCalcolare) {
    const auto& r = cripto::politica_di_rilascio();
    EXPECT_EQ(r.nuovi.memoria_kib, 256u * 1024u);
    EXPECT_EQ(r.nuovi.operazioni, 3u);
    EXPECT_EQ(cripto::controlla_parametri(r.nuovi, r), cripto::EsitoParametri::Ammessi);
    EXPECT_EQ(cripto::controlla_parametri({3, 64 * 1024}, r), cripto::EsitoParametri::TroppoBassi);
    EXPECT_EQ(cripto::controlla_parametri({1, 256 * 1024}, r), cripto::EsitoParametri::TroppoBassi);
    EXPECT_EQ(cripto::controlla_parametri({3, 0xFFFFFFFFu}, r), cripto::EsitoParametri::Assurdi);
    EXPECT_EQ(cripto::controlla_parametri({0xFFFFFFFFu, 256 * 1024}, r), cripto::EsitoParametri::Assurdi);
    EXPECT_EQ(cripto::controlla_parametri({1, 8}, kProve), cripto::EsitoParametri::Ammessi);
    EXPECT_EQ(cripto::controlla_parametri({1, 4}, kProve), cripto::EsitoParametri::TroppoBassi);   // minimo della libreria
}

TEST(Cripto, BusteSoloPerIlDestinatario) {
    const cripto::CoppiaBox u = cripto::nuova_coppia_box();
    const cripto::CoppiaBox v = cripto::nuova_coppia_box();
    EXPECT_EQ(cripto::pubblica_box(u.segreta), u.pubblica);
    const std::string k = "chiave dei dati personali, 32 b!";
    const Bytes b = cripto::sigilla(reinterpret_cast<const unsigned char*>(k.data()), k.size(), u.pubblica);
    Segreto out;
    ASSERT_TRUE(cripto::apri_busta(b, u.pubblica, u.segreta, out));
    EXPECT_EQ(out.testo(), k);
    EXPECT_FALSE(cripto::apri_busta(b, v.pubblica, v.segreta, out));
    Bytes x = b;
    x.back() ^= 1;
    EXPECT_FALSE(cripto::apri_busta(x, u.pubblica, u.segreta, out));
}

TEST(Cripto, AutenticazioneScopreOgniCambiamento) {
    const Segreto k(cripto::casuali(cripto::kChiave).data(), cripto::kChiave);
    const Bytes m = {'i', 'n', 't', 'e', 's', 't', 'a'};
    const Bytes t = cripto::autentica(k, m);
    ASSERT_EQ(t.size(), cripto::kAutenticazione);
    EXPECT_TRUE(cripto::verifica_autenticazione(t, m, k));
    Bytes m2 = m;
    m2[0] ^= 1;
    EXPECT_FALSE(cripto::verifica_autenticazione(t, m2, k));
    Bytes t2 = t;
    t2.back() ^= 0x80;
    EXPECT_FALSE(cripto::verifica_autenticazione(t2, m, k));
    const Segreto altra(cripto::casuali(cripto::kChiave).data(), cripto::kChiave);
    EXPECT_FALSE(cripto::verifica_autenticazione(t, m, altra));
    EXPECT_FALSE(cripto::verifica_autenticazione(Bytes(t.begin(), t.end() - 1), m, k));
}

TEST(Cripto, ImpronteChiaviDerivateEsadecimale) {
    EXPECT_EQ(cripto::esadecimale(cripto::impronta(std::string_view(""))),
              "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8");   // BLAKE2b-256 del testo vuoto
    const Segreto k(cripto::casuali(cripto::kChiave).data(), cripto::kChiave);
    const Segreto a = cripto::derivata(k, 1, "KZregist");
    const Segreto b = cripto::derivata(k, 2, "KZregist");
    EXPECT_FALSE(cripto::uguali(a.dati(), b.dati(), cripto::kChiave));
    Bytes x;
    EXPECT_TRUE(cripto::da_esadecimale("00ff10", x));
    EXPECT_EQ(x, (Bytes{0x00, 0xff, 0x10}));
    EXPECT_FALSE(cripto::da_esadecimale("0g", x));
    EXPECT_FALSE(cripto::da_esadecimale("abc", x));
}

TEST(Cripto, SegretoSiSpostaESiAccorcia) {
    Segreto a = segreto("abcdef");
    Segreto b = std::move(a);
    EXPECT_EQ(b.testo(), "abcdef");
    EXPECT_TRUE(a.vuoto());
    b.usa(3);
    EXPECT_EQ(b.testo(), "abc");
    EXPECT_THROW(b.usa(10), std::logic_error);
}

TEST(Cripto, NienteCoreDumpConLaCassaforteAperta) {
    EXPECT_TRUE(cripto::niente_core_dump());
    EXPECT_FALSE(cripto::core_dump_possibile());
}

// --- Password ----------------------------------------------------------------------------------------------------------

namespace {
password::Problema prova(const std::string& p, const std::vector<std::string>& contesto = {"U-GERVASIO", "DEMO-2026"}) {
    password::Problema problema = password::Problema::Nessuno;
    std::size_t caratteri = 0;
    (void)password::nuova(segreto(p), contesto, problema, caratteri);
    return problema;
}
}  // namespace

TEST(Password, AlmenoQuindiciCaratteriQualunqueCarattere) {
    EXPECT_EQ(prova("quattordici ca"), password::Problema::TroppoCorta);          // 14
    EXPECT_EQ(prova("quindici carat."), password::Problema::Nessuno);              // 15, nessun obbligo di cifre o maiuscole
    EXPECT_EQ(prova("perché sì, però più"), password::Problema::Nessuno);          // lettere accentate
    EXPECT_EQ(prova("Ζεύς και Ἥρα στον Όλυμπο"), password::Problema::Nessuno);     // un altro alfabeto
    // Ogni carattere conta uno: 14 lettere accentate (28 byte) sono troppo poche.
    EXPECT_EQ(prova("èèèàààòòòùùùìé"), password::Problema::TroppoCorta);
    EXPECT_EQ(prova("ciao 🙂 a tutti voi"), password::Problema::Nessuno);
    EXPECT_EQ(prova(std::string(256, 'x') + ""), password::Problema::Prevedibile);    // 256 ammessi, ma ripetuto
    std::string lunga;
    for (int i = 0; i < 257; ++i) lunga += static_cast<char>('a' + (i * 7) % 26);
    EXPECT_EQ(prova(lunga), password::Problema::TroppoLunga);
    EXPECT_EQ(prova(lunga.substr(0, 256)), password::Problema::Nessuno);
}

TEST(Password, CaratteriNonAmmessi) {
    EXPECT_EQ(prova("con una\ttabulazione"), password::Problema::CarattereNonAmmesso);
    EXPECT_EQ(prova(std::string("non utf8 \xff\xfe ancora")), password::Problema::NonUtf8);
    EXPECT_EQ(prova("carattere \xf3\xb0\x80\x80 privato ok"), password::Problema::Nessuno);   // uso privato: ammesso
    EXPECT_EQ(prova("non assegnato \xf0\xaf\xbf\xbe qui"), password::Problema::CarattereNonAmmesso);
    EXPECT_EQ(prova(""), password::Problema::Vuota);
}

TEST(Password, ElencoDellePasswordComuni) {
    EXPECT_GT(password::password_comuni(), 10000u);
    EXPECT_EQ(prova("1qaz2wsx3edc4rfv"), password::Problema::Comune);
    EXPECT_EQ(prova("1QAZ2WSX3EDC4RFV"), password::Problema::Comune);              // maiuscole e minuscole uguali
    EXPECT_EQ(prova("momsanaladventure"), password::Problema::Comune);
    EXPECT_EQ(prova("momsanaladventure!"), password::Problema::Nessuno);           // la password intera, non pezzi
}

TEST(Password, ContestoRipetizioniSequenze) {
    EXPECT_EQ(prova("kzerokzerokzero"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("KZero-Cassaforte 2026"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("u-gervasio u-gervasio 2026"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("demo-2026 password"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("abcabcabcabcabcabc"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("abcdefghijklmnopq"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("987654321098765"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("aaaaaaaaaaaaaaaa"), password::Problema::Comune);      // anche nell'elenco
    EXPECT_EQ(prova("ŵŵŵŵŵŵŵŵŵŵŵŵŵŵŵŵ"), password::Problema::Prevedibile);
    EXPECT_EQ(prova("gervasio va al lago di garda"), password::Problema::Nessuno);
}

TEST(Password, LaStessaPasswordConLettereAccentateScritteInDueModi) {
    // "è" come un carattere solo (U+00E8) e come "e" + accento (U+0300): stessa password normalizzata (NFC), stessa chiave.
    const std::string composta = "caff\xc3\xa8 nero bollente";
    const std::string scomposta = "caffe\xcc\x80 nero bollente";
    password::Problema a = password::Problema::Nessuno, b = password::Problema::Nessuno;
    const Segreto na = password::normalizza(segreto(composta), a);
    const Segreto nb = password::normalizza(segreto(scomposta), b);
    ASSERT_EQ(a, password::Problema::Nessuno);
    ASSERT_EQ(b, password::Problema::Nessuno);
    EXPECT_EQ(na.testo(), nb.testo());
    EXPECT_EQ(na.testo(), composta);
    const Bytes sale(cripto::kSale, 7);
    const Segreto ka = cripto::deriva(na, sale.data(), {1, 64});
    const Segreto kb = cripto::deriva(nb, sale.data(), {1, 64});
    EXPECT_TRUE(cripto::uguali(ka.dati(), kb.dati(), cripto::kChiave));
    // Maiuscole e minuscole contano per la chiave (si normalizza solo la forma, non le maiuscole).
    password::Problema c = password::Problema::Nessuno;
    EXPECT_NE(password::normalizza(segreto("CAFF\xc3\x88 nero bollente"), c).testo(), na.testo());
}

TEST(Password, Unicode) {
    EXPECT_STREQ(unicode::versione_unicode(), "17.0.0");
}
