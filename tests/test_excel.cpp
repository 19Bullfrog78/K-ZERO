// K-Zero - prove del lettore dei file Excel (.xlsx) che arrivano da fuori (K-019, src/api/excel.hpp): forma normale, file
// rotti e file preparati apposta (archivio troncato, cifrato, su piu' dischi, nomi ripetuti, bomba di compressione, dimensioni
// dichiarate false, DOCTYPE ed entita', XML malformato, limiti). I file si costruiscono da parti di testo (tests/xlsx_prove.hpp).
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "excel.hpp"
#include "xlsx_prove.hpp"

using namespace kzero::api::excel;
namespace x = kzero::prove::xlsx;

namespace {

using Motivo = ErroreExcel::Motivo;

Motivo motivo_di(const std::string& file, const Limiti& l = limiti(), const std::string& foglio = "") {
    try {
        leggi_foglio(file, foglio, l);
    } catch (const ErroreExcel& e) {
        return e.motivo;
    }
    ADD_FAILURE() << "nessun errore";
    return Motivo::NonXlsx;
}

std::string testo(const Foglio& f, std::uint32_t riga, std::uint32_t colonna) {
    for (const Riga& r : f.righe)
        if (r.numero == riga)
            for (const Cella& c : r.celle)
                if (c.colonna == colonna) return c.testo;
    return "(manca)";
}

const Cella* cella(const Foglio& f, std::uint32_t riga, std::uint32_t colonna) {
    for (const Riga& r : f.righe)
        if (r.numero == riga)
            for (const Cella& c : r.celle)
                if (c.colonna == colonna) return &c;
    return nullptr;
}

x::Archivio con_foglio_xml(const std::string& foglio_xml) {
    std::vector<x::Parte> p = x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}});
    for (x::Parte& q : p)
        if (q.nome == "xl/worksheets/sheet1.xml") q.dati = foglio_xml;
    return x::Archivio{p, 0};
}

const std::string kInizioFoglio = "<?xml version=\"1.0\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";

}  // namespace

TEST(Excel, FoglioNormale) {
    for (int metodo : {0, 8}) {
        const std::string f = x::file_righe({{"NOME", "CIVICO", "CLASSE"}, {"ROSSI ANNA  ", "21/C", "#3"}, {}, {"BIANCHI LUCA", "#8", "!"}},
                                           metodo);
        const Foglio s = leggi_foglio(f, "");
        ASSERT_EQ(s.righe.size(), 3u);   // la riga vuota non c'e'
        EXPECT_EQ(testo(s, 1, 1), "NOME");
        EXPECT_EQ(testo(s, 2, 1), "ROSSI ANNA  ");   // spazi in fondo come nel file: li toglie chi legge le righe
        EXPECT_EQ(testo(s, 2, 2), "21/C");
        EXPECT_EQ(cella(s, 2, 3)->tipo, Cella::Tipo::Numero);
        EXPECT_EQ(testo(s, 2, 3), "3");
        EXPECT_EQ(s.righe[2].numero, 4u);
        EXPECT_EQ(cella(s, 4, 3)->tipo, Cella::Tipo::Errore);
    }
}

TEST(Excel, TestiInLineaFormuleEEscape) {
    x::Foglio g{"Elenco", {{"A_x000D_B", "=CONCATENA(X)|valore salvato"}, {"x&y <z>"}}, true};
    const Foglio s = leggi_foglio(x::file_xlsx(std::vector<x::Foglio>{g}), "elenco");   // il nome del foglio senza maiuscole
    EXPECT_EQ(testo(s, 1, 1), "A\rB");
    EXPECT_EQ(testo(s, 1, 2), "valore salvato");   // la formula non si calcola: solo il valore salvato
    EXPECT_EQ(testo(s, 2, 1), "x&y <z>");
    EXPECT_EQ(decodifica_escape("_x005F_x000D_"), "_x000D_");
    EXPECT_EQ(decodifica_escape("_xD83D__xDE00_"), "\xF0\x9F\x98\x80");
    EXPECT_EQ(decodifica_escape("_xZZZZ_"), "_xZZZZ_");
}

TEST(Excel, FoglioScelto) {
    const std::string f = x::file_xlsx(std::vector<x::Foglio>{x::Foglio{"Primo", {{"uno"}}, false}, x::Foglio{"Secondo", {{"due"}}, false}});
    EXPECT_EQ(testo(leggi_foglio(f, ""), 1, 1), "uno");
    EXPECT_EQ(testo(leggi_foglio(f, "SECONDO"), 1, 1), "due");
    EXPECT_EQ(motivo_di(f, limiti(), "Terzo"), Motivo::FoglioMancante);
}

TEST(Excel, NonXlsxEVecchioXls) {
    EXPECT_EQ(motivo_di("nome;cognome\n"), Motivo::NonXlsx);
    EXPECT_EQ(motivo_di(""), Motivo::NonXlsx);
    EXPECT_EQ(motivo_di(std::string("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) + std::string(500, '\0')), Motivo::VecchioXls);
}

TEST(Excel, ArchivioTroncatoORovinato) {
    const std::string f = x::file_righe({{"a", "b"}});
    for (std::size_t n : {f.size() / 2, f.size() - 10, std::size_t{30}}) EXPECT_EQ(motivo_di(f.substr(0, n)), Motivo::ArchivioRotto) << n;
    // Impronta sbagliata e dimensione dichiarata diversa da quella vera: la parte non si usa.
    for (int caso = 0; caso < 2; ++caso) {
        x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
        for (x::Parte& p : a.parti)
            if (p.nome == "xl/worksheets/sheet1.xml") {
                if (caso == 0) p.crc_sbagliato = true;
                else p.veri_in_piu = 5;
            }
        EXPECT_EQ(motivo_di(a.scrivi()), Motivo::ArchivioRotto) << caso;
    }
}

// Caso trovato dal fuzzing (K-019): la directory centrale manda una parte oltre la fine del file. La lettura dalla memoria di
// miniz calcolava l'indirizzo fuori dal file (comportamento non definito, visto dal sanitizer); ora la legge una funzione di
// K-Zero che controlla prima la posizione. Con il sanitizer questa prova si ferma se torna il problema.
TEST(Excel, ParteOltreLaFineDelFile) {
    for (std::uint32_t oltre : {100000u, 0x7FFFFFF0u, 0xFFFFFF00u}) {
        x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
        for (x::Parte& p : a.parti)
            if (p.nome == "xl/worksheets/sheet1.xml") p.posizione_in_piu = oltre;
        EXPECT_EQ(motivo_di(a.scrivi()), Motivo::ArchivioRotto) << oltre;
    }
}

TEST(Excel, CifratoPiuDischiNomiRipetutiCompressione) {
    x::Archivio cifrato{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    cifrato.parti[2].bit_in_piu = 1;
    EXPECT_EQ(motivo_di(cifrato.scrivi()), Motivo::Cifrato);
    x::Archivio dischi{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 1};
    EXPECT_EQ(motivo_di(dischi.scrivi()), Motivo::PiuDischi);
    x::Archivio ripetuti{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    x::Parte doppia = ripetuti.parti[2];
    doppia.nome = "XL/Worksheets/Sheet1.xml";   // lo stesso nome: il formato non distingue le maiuscole
    ripetuti.parti.push_back(doppia);
    EXPECT_EQ(motivo_di(ripetuti.scrivi()), Motivo::NomiRipetuti);
    x::Archivio bzip{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    bzip.parti[2].metodo = 12;
    bzip.parti[2].compressi = "qualcosa";
    bzip.parti[2].veri = 8;
    EXPECT_EQ(motivo_di(bzip.scrivi()), Motivo::CompressioneNonAmmessa);
}

// Bomba: circa 1.000 byte veri per byte compresso. Con i limiti del programma: oltre 1 MB vero, il rapporto 200 la ferma.
TEST(Excel, BombaDiCompressione) {
    x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    for (x::Parte& p : a.parti)
        if (p.nome == "xl/worksheets/sheet1.xml") {
            p.compressi = x::deflate_bomba('a', 20000);   // 5,16 MB veri da circa 5 KB
            p.veri = 1 + 258ull * 20000;
            p.crc = 0;
        }
    EXPECT_EQ(motivo_di(a.scrivi()), Motivo::Bomba);
    // Una parte grande ma normale (meno di 1 MB vero) non e' una bomba: arriva fino al controllo dell'impronta (qui sbagliata).
    for (x::Parte& p : a.parti)
        if (p.nome == "xl/worksheets/sheet1.xml") {
            p.compressi = x::deflate_bomba('a', 3000);
            p.veri = 1 + 258ull * 3000;
        }
    EXPECT_EQ(motivo_di(a.scrivi()), Motivo::ArchivioRotto);
}

// I limiti si controllano sui byte veri decompressi, mai su quelli dichiarati: una parte che dichiara 1.000 byte e ne da' di piu'
// si ferma al limite della parte (rapporto spento per provare solo quello), e il limite del totale vale su tutte le parti lette.
TEST(Excel, LimitiSuiByteVeri) {
    Limiti l;
    l.decompresso_parte = 1024 * 1024;
    l.rapporto_compressione = 1000000;
    x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    for (x::Parte& p : a.parti)
        if (p.nome == "xl/worksheets/sheet1.xml") {
            p.compressi = x::deflate_bomba('a', 10000);   // 2,58 MB veri, dichiarati
            p.veri = 1 + 258ull * 10000;
        }
    EXPECT_EQ(motivo_di(a.scrivi(), l), Motivo::TroppoGrande);
    // Dichiarati meno byte di quelli veri: la libreria si ferma dove finiscono quelli dichiarati (archivio rovinato), e
    // comunque i limiti contano i byte veri.
    for (x::Parte& p : a.parti)
        if (p.nome == "xl/worksheets/sheet1.xml") p.veri = 1000;
    EXPECT_EQ(motivo_di(a.scrivi(), l), Motivo::ArchivioRotto);
    Limiti t;
    t.decompresso_totale = 1000;   // le parti lette di un piccolo file superano 1.000 byte in tutto
    EXPECT_EQ(motivo_di(x::file_righe({{"a"}}), t), Motivo::TroppoGrande);
    Limiti f;
    f.file_byte = 100;
    EXPECT_EQ(motivo_di(x::file_righe({{"a"}}), f), Motivo::TroppoGrande);
    Limiti parti;
    parti.parti = 3;
    EXPECT_EQ(motivo_di(x::file_righe({{"a"}}), parti), Motivo::TroppeParti);
}

// Con i limiti del programma: una parte di oltre 100 MB veri si ferma (qui anche come bomba: tutte e due le difese).
TEST(Excel, ParteOltreIlMassimoDelProgramma) {
    x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
    for (x::Parte& p : a.parti)
        if (p.nome == "xl/worksheets/sheet1.xml") {
            p.compressi = x::deflate_bomba('a', 410000);   // 101 MB veri da circa 100 KB
            p.veri = 1 + 258ull * 410000;
        }
    const Motivo m = motivo_di(a.scrivi());
    EXPECT_TRUE(m == Motivo::Bomba || m == Motivo::TroppoGrande);
    Limiti senza_rapporto;
    senza_rapporto.rapporto_compressione = 1000000;
    EXPECT_EQ(motivo_di(a.scrivi(), senza_rapporto), Motivo::TroppoGrande);
}

TEST(Excel, DoctypeEdEntita) {
    // La "bomba delle entita'" (billion laughs) e un'entita' esterna: rifiutate al DOCTYPE, prima di qualunque entita'.
    const std::string risate = "<?xml version=\"1.0\"?><!DOCTYPE w [<!ENTITY a \"aaaaaaaaaa\"><!ENTITY b \"&a;&a;&a;&a;&a;&a;\">]>"
                               "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>&b;</t></is></c></row>"
                               "</sheetData></worksheet>";
    EXPECT_EQ(motivo_di(con_foglio_xml(risate).scrivi()), Motivo::Doctype);
    const std::string esterna = "<?xml version=\"1.0\"?><!DOCTYPE w SYSTEM \"file:///etc/passwd\"><worksheet/>";
    EXPECT_EQ(motivo_di(con_foglio_xml(esterna).scrivi()), Motivo::Doctype);
    // Senza DOCTYPE un'entita' sconosciuta e' XML malformato (la libreria e' compilata senza entita').
    const std::string sconosciuta = kInizioFoglio + "<sheetData><row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>&x;</t></is></c></row>"
                                                    "</sheetData></worksheet>";
    EXPECT_EQ(motivo_di(con_foglio_xml(sconosciuta).scrivi()), Motivo::XmlMalformato);
}

TEST(Excel, XmlMalformato) {
    EXPECT_EQ(motivo_di(con_foglio_xml(kInizioFoglio + "<sheetData><row r=\"1\">").scrivi()), Motivo::XmlMalformato);
    EXPECT_EQ(motivo_di(con_foglio_xml("non e' xml").scrivi()), Motivo::XmlMalformato);
    EXPECT_EQ(motivo_di(con_foglio_xml("").scrivi()), Motivo::XmlMalformato);
    std::string profondo = kInizioFoglio;
    for (int i = 0; i < 100; ++i) profondo += "<a>";
    for (int i = 0; i < 100; ++i) profondo += "</a>";
    EXPECT_EQ(motivo_di(con_foglio_xml(profondo + "</worksheet>").scrivi()), Motivo::XmlMalformato);
}

TEST(Excel, PartiMancanti) {
    for (const char* togli : {"_rels/.rels", "xl/workbook.xml", "xl/worksheets/sheet1.xml", "xl/sharedStrings.xml"}) {
        x::Archivio a{x::parti_xlsx({x::Foglio{"Foglio1", {{"a"}}, false}}), 0};
        a.parti.erase(std::remove_if(a.parti.begin(), a.parti.end(), [&](const x::Parte& p) { return p.nome == togli; }), a.parti.end());
        EXPECT_EQ(motivo_di(a.scrivi()), Motivo::ParteMancante) << togli;
    }
    x::Archivio senza_fogli{x::parti_xlsx({}), 0};
    EXPECT_EQ(motivo_di(senza_fogli.scrivi()), Motivo::FoglioMancante);
}

TEST(Excel, CelleERighe) {
    // Celle senza valore (solo formattazione) ignorate anche lontano; riferimenti senza "r" in fila; formula mai calcolata = vuota.
    const std::string s = kInizioFoglio + "<sheetData><row r=\"2\"><c><v>1</v></c><c><v>2</v></c>"
                                          "<c t=\"str\"><f>A1</f></c><c r=\"XFD2\" s=\"3\"/></row><row><c t=\"b\"><v>1</v></c></row></sheetData></worksheet>";
    const Foglio f = leggi_foglio(con_foglio_xml(s).scrivi(), "");
    ASSERT_EQ(f.righe.size(), 2u);
    EXPECT_EQ(testo(f, 2, 1), "1");
    EXPECT_EQ(testo(f, 2, 2), "2");
    EXPECT_EQ(f.righe[0].celle.size(), 2u);
    EXPECT_EQ(cella(f, 3, 1)->tipo, Cella::Tipo::VeroFalso);
    // Riferimenti sbagliati, testo condiviso che non c'e', cella ripetuta.
    for (const std::string& rotto : {std::string("<row r=\"1\"><c r=\"A2\"><v>1</v></c></row>"), std::string("<row r=\"x\"></row>"),
                                     std::string("<row r=\"1\"><c r=\"A1\" t=\"s\"><v>99</v></c></row>"),
                                     std::string("<row r=\"1\"><c r=\"A1\"><v>1</v></c><c r=\"A1\"><v>2</v></c></row>"),
                                     std::string("<row r=\"1\"><c r=\"A1\"><v>1</v></c></row><row r=\"1\"><c r=\"B1\"><v>1</v></c></row>")})
        EXPECT_EQ(motivo_di(con_foglio_xml(kInizioFoglio + "<sheetData>" + rotto + "</sheetData></worksheet>").scrivi()),
                  Motivo::CellaRotta)
            << rotto;
}

TEST(Excel, LimitiDelFoglio) {
    Limiti l;
    l.righe = 3;
    l.colonne = 2;
    l.cella = 10;
    EXPECT_EQ(motivo_di(x::file_righe({{"a"}, {"b"}, {"c"}, {"d"}}), l), Motivo::TroppeRighe);
    EXPECT_EQ(motivo_di(x::file_righe({{"a", "b", "c"}}), l), Motivo::TroppeColonne);
    // Una cella troppo lunga non ferma il file: la cella lo dice, e chi legge le righe decide (riga illeggibile se serve).
    const Foglio f = leggi_foglio(x::file_righe({{"12345678901", "corto"}}), "", l);
    EXPECT_EQ(cella(f, 1, 1)->tipo, Cella::Tipo::TroppoLunga);
    EXPECT_TRUE(cella(f, 1, 1)->testo.empty());
    EXPECT_EQ(testo(f, 1, 2), "corto");
    Limiti testi;
    testi.stringhe_condivise = 1;
    EXPECT_EQ(motivo_di(x::file_righe({{"a", "b"}}), testi), Motivo::TroppiTesti);
}
