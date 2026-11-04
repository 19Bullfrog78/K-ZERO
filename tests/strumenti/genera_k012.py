# Genera tests/esempi/k012-servizi dal demo (dati inventati): il pacchetto di prova dei tipi di servizio (K-012).
# Uso, dalla cartella del repository: python3 tests/strumenti/genera_k012.py esempi/demo tests/esempi/k012-servizi
# Parte dal pacchetto di prova degli orari, rifatto sul demo di oggi con tests/strumenti/genera_k011.py (cosi' ogni campo
# personale o particolare degli schemi delle ultime versioni compare in un pacchetto delle prove), poi aggiunge i casi del
# K-012: una gita scolastica NCC con studenti, l'NCC con un'auto da 8 posti (niente Reg. 561) e con un pullman (561 certo,
# interruzione di 15 + 15 minuti), un servizio "altro" (servizio occasionale da confermare), i committenti privati di un
# servizio verso un luogo sensibile (nome legato) e verso un luogo normale (nome personale); dal K-013 un'azienda senza scelte del
# tachigrafo per tipo di servizio con una persona in prestito a un'azienda che le ha.
# Il pacchetto generato e' nel repository: dopo un cambio del demo o di questo script si rigenera e si rivedono le prove
# che lo usano (tests/CMakeLists.txt, sezione K-012). tests/esempi/k011-orari invece resta quello del K-011 (file delle
# versioni 2): e' la prova che le versioni 2 si leggono ancora con lo stesso esito. Nessun dato vero.
import json, os, subprocess, sys, copy
SRC, DST = sys.argv[1], sys.argv[2]
subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'genera_k011.py'), SRC, DST], check=True)
def load(n): return json.load(open(f'{DST}/{n}.json', encoding='utf-8'))
def save(n, d):
    with open(f'{DST}/{n}.json', 'w', encoding='utf-8') as f:
        json.dump(d, f, ensure_ascii=False, indent=2); f.write('\n')
def uno(elenco, codice, campo='codice'): return [x for x in elenco if x[campo] == codice][0]

pk = load('pacchetto')
pk['codice'] = 'K012-SERVIZI'
pk['nome'] = "Prova dei tipi di servizio (K-012)"
pk['origine']['nota'] = ("Dati inventati per le prove dei tipi di servizio (K-012), dal pacchetto di prova degli orari (K-011) rifatto sul "
                        "demo: gita scolastica NCC, NCC con un'auto da 8 posti e con un pullman, un servizio 'altro', committenti privati.")
save('pacchetto', pk)

GIORNI = ["lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"]
ca, tu, sf = load('calendari'), load('turni'), load('servizi_fissi')
co, se, me, pe = load('corse'), load('servizi'), load('mezzi'), load('persone')
cm, ct = load('committenti'), load('contratti')

def turno(codice, nome, persona, data, elementi, mezzo, azienda='ALFA'):
    cal = f'CAL-{codice}'
    ca['calendari'].append({"codice": cal, "nome": f"Solo il {data}", "dal": data, "al": data, "giorni": GIORNI})
    rimessa = 'L-RIMESSA-BETA' if azienda == 'BETA' else 'L-RIMESSA-ALFA'
    tu['turni'].append({"codice": codice, "nome": nome, "azienda": azienda, "ruolo": "autista", "calendario": cal,
                        "mezzo": {"modo": "preciso", "codice": mezzo}, "inizio": rimessa, "fine": rimessa, "elementi": elementi})
    sf['servizi_fissi'].append({"codice": f'SF-{codice}', "turno": codice, "persona": persona, "calendario": cal})
    return cal
def att(tipo, luogo, dalle, alle, verso=None, km=None):
    a = {"attivita": tipo, "luogo": luogo, "dalle": dalle, "alle": alle}
    if verso: a["verso"] = verso
    if km is not None: a["km_vuoti"] = km
    return a
def c(codice): return {"corsa": codice}
def corsa(codice, servizio, cal, da, dalle, a, alle, km, gruppo, persone, nome):
    co['corse'].append({"codice": codice, "nome": nome, "servizi": [servizio], "calendario": cal,
                        "passaggi": [{"luogo": da, "orario": dalle, "persone": [{"gruppo": gruppo, "salgono": persone}]},
                                     {"luogo": a, "orario": alle, "persone": [{"gruppo": gruppo, "scendono": persone}],
                                      "km": {"pieni": km}}]})
def gruppo(codice, nome, tipo, verso, da, a, persone, scuola=None, descrizione=None):
    g = {"codice": codice, "nome": nome, "tipo": tipo}
    if descrizione: g["tipo_descrizione"] = descrizione
    g.update({"verso": verso})
    if scuola: g["scuola"] = scuola
    g.update({"conteggi": "esatti", "fermate": [{"luogo": da, "salgono": persone}, {"luogo": a, "scendono": persone}]})
    return g
def servizio(codice, nome, tipo, contratto, cal, gruppi, descrizione=None):
    s = {"codice": codice, "nome": nome, "tipo": tipo}
    if descrizione: s["tipo_descrizione"] = descrizione
    s.update({"contratto": contratto, "calendario": cal, "liberta_oraria": {"tipo": "fissa"}, "domanda": {"gruppi": gruppi}})
    se['servizi'].append(s)

# Committenti (enti inventati) e contratti dei servizi nuovi.
cm['committenti'] += [{"codice": "IC-VALFONDA", "nome": "Istituto comprensivo di Valfonda (inventato)"},
                      {"codice": "CARTIERA", "nome": "Cartiera di Valfonda (inventata)"}]
def contratto(codice, nome, committente, azienda, regolamento='R-INV'):
    ct['contratti'].append({"codice": codice, "nome": nome, "committente": committente, "azienda": azienda, "regolamento": regolamento,
                            "valido_dal": "2026-09-01", "valido_al": "2027-08-31", "assistente_fornito_da": "committente"})
contratto('CT-GITE-SCUOLA', "Gite scolastiche dell'istituto comprensivo", 'IC-VALFONDA', 'ALFA', 'R-TN')
contratto('CT-NCC-ALFA', "Noleggi con conducente di Alfa", 'PROLOCO', 'ALFA')
contratto('CT-NCC-BETA', "Noleggi con conducente di Beta", 'PROLOCO', 'BETA')
contratto('CT-NAVETTA', "Navetta degli operai", 'CARTIERA', 'ALFA')

# Mezzi: un minibus da 16 posti, un'auto da 8 posti (9 persone con il conducente: niente Reg. 561) e un pullman da 50 posti,
# tutti con l'uso NCC.
proto_auto, proto_bus = uno(me['mezzi'], 'ALFA-04'), uno(me['mezzi'], 'ALFA-01')
for codice, proto, azienda, cat, posti, targa in [('V-NCC-16', proto_bus, 'ALFA', 'M2', 16, 'AA 916 NC'),
                                                  ('V-NCC-8', proto_auto, 'BETA', 'M1', 8, 'BB 908 NC'),
                                                  ('V-NCC-50', proto_bus, 'ALFA', 'M3', 50, 'AA 950 NC')]:
    x = copy.deepcopy(proto)
    for k in ['autorizzazioni_uso', 'vincolo_destinazione', 'carrozzine', 'pedana', 'tempo_pedana_secondi', 'posti_persi_per_carrozzina']:
        x.pop(k, None)
    x.update({"codice": codice, "azienda": azienda, "uso": "NCC", "targa": targa, "categoria": cat, "posti": posti, "posti_in_piedi": 0})
    me['mezzi'].append(x)

# Persone: autisti con le abilitazioni del demo e il certificato KB per l'auto a noleggio.
pa, pb = uno(pe['persone'], 'P-A'), uno(pe['persone'], 'P-B1')
def persona(codice, nome, da):
    abil = copy.deepcopy(da['abilitazioni'])
    if not any(a['tipo'] == 'KB' for a in abil): abil.append({"tipo": "KB"})
    pe['persone'].append({"codice": codice, "azienda": da['azienda'], "ruoli": ["autista"], "nome": nome,
                          "presa_servizio": da['presa_servizio'], "rapporto": {"tipo": "dipendente_tempo_pieno"}, "abilitazioni": abil})
persona('P-GITA-SCUOLA', "Gita Scolastica", pa)
persona('P-NCC-AUTO', "Noleggio Auto", pb)
persona('P-NCC-BUS', "Noleggio Pullman", pa)
persona('P-NAVETTA', "Navetta Operai", pa)

# 1. Gita scolastica NCC (venerdi' 20 novembre): studenti delle medie al lago e ritorno, con il minibus da 16 posti. La
# categoria della corsa resta NCC (non scolastico) e il Reg. 561 vale con certezza (non e' un servizio scolastico fino a 50 km).
cal = turno('T-GITA-SCUOLA', "Gita scolastica al lago", 'P-GITA-SCUOLA', '2026-11-20',
            [att('PRESA', 'L-RIMESSA-ALFA', '08:00', '08:10'), att('VUOTO', 'L-RIMESSA-ALFA', '08:10', '08:25', 'L-MEDIE', 4.0),
             c('C-GITA-SCUOLA-A'), att('ATTESA', 'F-LAGOBELLO', '09:15', '15:00'), c('C-GITA-SCUOLA-R'),
             att('VUOTO', 'L-MEDIE', '15:45', '16:00', 'L-RIMESSA-ALFA', 4.0), att('FINE', 'L-RIMESSA-ALFA', '16:00', '16:10')], 'V-NCC-16')
servizio('S-GITA-SCUOLA', "Gita scolastica delle medie al lago", 'ncc', 'CT-GITE-SCUOLA', cal,
         [gruppo('G-GITA-SCUOLA-A', "Medie, andata della gita", 'studenti', 'andata', 'L-MEDIE', 'F-LAGOBELLO', 14, 'SC-MEDIE'),
          gruppo('G-GITA-SCUOLA-R', "Medie, ritorno della gita", 'studenti', 'ritorno', 'F-LAGOBELLO', 'L-MEDIE', 14, 'SC-MEDIE')])
corsa('C-GITA-SCUOLA-A', 'S-GITA-SCUOLA', cal, 'L-MEDIE', '08:30', 'F-LAGOBELLO', '09:15', 30.0, 'G-GITA-SCUOLA-A', 14, "Gita scolastica, andata")
corsa('C-GITA-SCUOLA-R', 'S-GITA-SCUOLA', cal, 'F-LAGOBELLO', '15:00', 'L-MEDIE', '15:45', 30.0, 'G-GITA-SCUOLA-R', 14, "Gita scolastica, ritorno")

# 2. NCC con l'auto da 8 posti di Beta (sabato 7 novembre): niente Reg. 561 e niente L. 138 (e' per le linee). Per un
# dipendente nessuna legge sugli orari nei dati: l'avviso K-CTR-107, come per gli altri servizi fuori dalla L. 138.
cal = turno('T-NCC-AUTO', "Noleggio con l'auto", 'P-NCC-AUTO', '2026-11-07',
            [att('PRESA-SERVIZIO', 'L-RIMESSA-BETA', '08:30', '08:45'), att('RIENTRO', 'L-RIMESSA-BETA', '08:45', '09:00', 'F-SORGENTA', 3.0),
             c('C-NCC-AUTO-A'), c('C-NCC-AUTO-R'), att('RIENTRO', 'F-SORGENTA', '16:40', '16:55', 'L-RIMESSA-BETA', 3.0)],
            'V-NCC-8', 'BETA')
servizio('S-NCC-AUTO', "Noleggio con l'auto al lago", 'ncc', 'CT-NCC-BETA', cal,
         [gruppo('G-NCC-AUTO-A', "Clienti, andata", 'altro', 'andata', 'F-SORGENTA', 'F-LAGOBELLO', 4, descrizione="Clienti del noleggio"),
          gruppo('G-NCC-AUTO-R', "Clienti, ritorno", 'altro', 'ritorno', 'F-LAGOBELLO', 'F-SORGENTA', 4, descrizione="Clienti del noleggio")])
corsa('C-NCC-AUTO-A', 'S-NCC-AUTO', cal, 'F-SORGENTA', '09:00', 'F-LAGOBELLO', '09:40', 25.0, 'G-NCC-AUTO-A', 4, "Noleggio con l'auto, andata")
corsa('C-NCC-AUTO-R', 'S-NCC-AUTO', cal, 'F-LAGOBELLO', '16:00', 'F-SORGENTA', '16:40', 25.0, 'G-NCC-AUTO-R', 4, "Noleggio con l'auto, ritorno")

# 3. NCC con il pullman da 50 posti (sabato 14 novembre) e 4. lo stesso giro come navetta "altro" (giovedi' 19 novembre):
# 3 ore e 15 di guida, poi due interruzioni di 15 minuti, poi 1 ora e 45 (piu' i trasferimenti). Per il Reg. 561 (art. 7)
# 15 + 15 azzera il conto solo per i servizi occasionali: l'NCC lo e' con certezza (nessun avviso), "altro" solo da
# confermare (un avviso K-CTR-114 con il motivo; nessun errore).
def giro_lungo(sigla, servizio_, cal):
    g = f'G-{sigla}'
    corsa(f'C-{sigla}-1', servizio_, cal, 'F-STAZIONE', '06:00', 'F-LAGOBELLO', '08:30', 140.0, g, 30, "Andata lunga")
    corsa(f'C-{sigla}-2', servizio_, cal, 'F-LAGOBELLO', '08:45', 'F-PRADELLO', '09:15', 25.0, g + '-2', 30, "Tratto breve")
    corsa(f'C-{sigla}-3', servizio_, cal, 'F-PRADELLO', '09:30', 'F-STAZIONE', '11:15', 100.0, g + '-3', 30, "Ritorno")
    return [att('PRESA', 'L-RIMESSA-ALFA', '05:30', '05:45'), att('VUOTO', 'L-RIMESSA-ALFA', '05:45', '06:00', 'F-STAZIONE', 2.0),
            c(f'C-{sigla}-1'), att('PAUSA', 'F-LAGOBELLO', '08:30', '08:45'), c(f'C-{sigla}-2'), att('PAUSA', 'F-PRADELLO', '09:15', '09:30'),
            c(f'C-{sigla}-3'), att('VUOTO', 'F-STAZIONE', '11:15', '11:30', 'L-RIMESSA-ALFA', 2.0), att('FINE', 'L-RIMESSA-ALFA', '11:30', '11:40')]
def gruppi_giro(sigla, tipo, descrizione):
    return [gruppo(f'G-{sigla}', "Andata", tipo, 'andata', 'F-STAZIONE', 'F-LAGOBELLO', 30, descrizione=descrizione),
            gruppo(f'G-{sigla}-2', "Tratto breve", tipo, 'andata', 'F-LAGOBELLO', 'F-PRADELLO', 30, descrizione=descrizione),
            gruppo(f'G-{sigla}-3', "Ritorno", tipo, 'ritorno', 'F-PRADELLO', 'F-STAZIONE', 30, descrizione=descrizione)]
cal = 'CAL-T-NCC-BUS'
elementi = giro_lungo('NCC-BUS', 'S-NCC-BUS', cal)
turno('T-NCC-BUS', "Noleggio con il pullman", 'P-NCC-BUS', '2026-11-14', elementi, 'V-NCC-50')
servizio('S-NCC-BUS', "Escursione al lago con il pullman", 'ncc', 'CT-NCC-ALFA', cal, gruppi_giro('NCC-BUS', 'altro', "Turisti"))
cal = 'CAL-T-NAVETTA'
elementi = giro_lungo('NAVETTA', 'S-NAVETTA', cal)
turno('T-NAVETTA', "Navetta degli operai", 'P-NAVETTA', '2026-11-19', elementi, 'V-NCC-50')
servizio('S-NAVETTA', "Navetta degli operai della cartiera", 'altro', 'CT-NAVETTA', cal,
         gruppi_giro('NAVETTA', 'altro', "Operai della cartiera"),
         descrizione="Navetta casa-lavoro con tragitto e orari fissi (servizio regolare specializzato, non un noleggio)")

# 5. I committenti privati del demo (K-012): CP-01 noleggia per un matrimonio (luoghi normali: nome personale), CP-02 per andare
# al centro diurno (luogo sensibile: nome legato). Il pacchetto degli orari rifa' tutti i turni: si riprendono quelli del demo.
demo_turni = json.load(open(f'{SRC}/turni.json', encoding='utf-8'))['turni']
for codice, data in [('T-MATRIMONIO', '2027-07-10'), ('T-PRIVATO-CENTRO', '2027-07-07')]:
    t = copy.deepcopy(uno(demo_turni, codice))
    tu['turni'].append(t)
    sf['servizi_fissi'].append({"codice": f'SF-{codice}', "turno": codice, "persona": 'P-G', "calendario": t['calendario']})

# 6. K-013 (persona in prestito): Gamma (inventata) non ha scelte del tachigrafo per tipo di servizio. P-GAMMA-PRESTITO e' in
# prestito a Beta, che registra le linee con il tachigrafo (scelta per tipo): giovedi' 26 novembre fa due corse della linea B620
# (14,1 km: fuori dal Reg. 561 per legge, dentro per la scelta di Beta). P-GAMMA lavora solo per Gamma (una riserva lo stesso
# giorno). Senza i dati particolari la prima ha la riga fissa dei regimi seguiti per scelta (la scelta per tipo e' dell'azienda
# di un suo turno, chosen_by_kind), la seconda no.
pk['aziende'].append({"codice": "GAMMA", "nome": "Autolinee Gamma (inventata)"})
save('pacchetto', pk)
az = load('aziende')
gamma = copy.deepcopy(uno(az['aziende'], 'BETA', 'azienda'))
for k in ['modo_corse', 'nota', 'rientro', 'scelte_orari']:
    gamma.pop(k, None)
gamma['azienda'] = 'GAMMA'
gamma['attivita'].append({"codice": "RISERVA", "nome": "Riserva in rimessa", "usa_il_mezzo": False, "tempo": "disponibilita"})
gamma['nota'] = "Autolinee Gamma (inventata) non ha scelte del tachigrafo per tipo di servizio (K-013, persona in prestito)."
az['aziende'].append(gamma)
save('aziende', az)
for codice, nome, prestito in [('P-GAMMA-PRESTITO', "Gamma In Prestito", True), ('P-GAMMA', "Gamma Senza Prestito", False)]:
    x = {"codice": codice, "azienda": "GAMMA", "ruoli": ["autista"], "nome": nome, "presa_servizio": "L-RIMESSA-BETA",
         "rapporto": {"tipo": "dipendente_tempo_pieno"}, "abilitazioni": copy.deepcopy(pb['abilitazioni'])}
    if prestito:
        x["prestiti"] = [{"azienda": "BETA", "accordo": "Accordo di prestito n. 1/2026 tra Gamma e Beta (inventato)",
                          "dal": "2026-09-01", "al": "2027-08-31"}]
    pe['persone'].append(x)
bus = copy.deepcopy(uno(me['mezzi'], 'BETA-02'))
bus.update({"codice": "V-BETA-PRESTITO", "targa": "BB 926 BB"})
me['mezzi'].append(bus)
cal = turno('T-PRESTITO-GAMMA', "Linea B620 con una persona di Gamma", 'P-GAMMA-PRESTITO', '2026-11-26',
            [att('PRESA-SERVIZIO', 'L-RIMESSA-BETA', '07:30', '07:45'), att('RIENTRO', 'L-RIMESSA-BETA', '07:45', '08:00', 'F-SORGENTA', 3.0),
             c('C-PRESTITO-1'), c('C-PRESTITO-2'), att('RIENTRO', 'F-SORGENTA', '09:40', '09:55', 'L-RIMESSA-BETA', 3.0)],
            'V-BETA-PRESTITO', 'BETA')
for codice, ora, da, a in [('C-PRESTITO-1', ('08:00', '08:12', '08:30'), 'F-SORGENTA', 'F-MONTEBRINO'),
                           ('C-PRESTITO-2', ('09:00', '09:18', '09:40'), 'F-MONTEBRINO', 'F-SORGENTA')]:
    co['corse'].append({"codice": codice, "nome": "Linea B620, corsa con la persona di Gamma", "servizi": ["S-LIN-B620"], "calendario": cal,
                        "passaggi": [{"luogo": da, "orario": ora[0]}, {"luogo": "F-LAGOBELLO", "orario": ora[1], "km": {"pieni": 6.2 if da == 'F-SORGENTA' else 7.9}},
                                     {"luogo": a, "orario": ora[2], "km": {"pieni": 7.9 if da == 'F-SORGENTA' else 6.2}}],
                        "km_totali": {"pieni": 14.1, "vuoti": 0}})
cal = f'CAL-T-GAMMA'
ca['calendari'].append({"codice": cal, "nome": "Solo il 2026-11-26", "dal": "2026-11-26", "al": "2026-11-26", "giorni": GIORNI})
tu['turni'].append({"codice": "T-GAMMA", "nome": "Riserva di Gamma", "azienda": "GAMMA", "ruolo": "autista", "calendario": cal,
                    "mezzo": {"modo": "nessuno"}, "inizio": "L-RIMESSA-BETA", "fine": "L-RIMESSA-BETA",
                    "elementi": [att('RISERVA', 'L-RIMESSA-BETA', '08:00', '12:00')]})
sf['servizi_fissi'].append({"codice": "SF-T-GAMMA", "turno": "T-GAMMA", "persona": "P-GAMMA", "calendario": cal})

for n, d in [('calendari', ca), ('turni', tu), ('servizi_fissi', sf), ('corse', co), ('servizi', se), ('mezzi', me), ('persone', pe),
             ('committenti', cm), ('contratti', ct)]:
    save(n, d)
