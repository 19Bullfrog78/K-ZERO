# Genera tests/esempi/k011-orari dal demo (dati inventati): un pacchetto di prova degli orari di lavoro (K-011).
# Uso, dalla cartella del repository: python3 tests/strumenti/genera_k011.py esempi/demo tests/esempi/k011-orari
# Il pacchetto generato e' nel repository. Dal K-012 tests/esempi/k011-orari resta quello fatto con il demo del K-011 (file
# delle versioni 2: git show 9fe6cfb:esempi/demo) e non si rigenera piu': e' la prova che le versioni 2 si leggono ancora con
# lo stesso esito. Sul demo di oggi questo script fa la base del pacchetto di prova dei servizi (tests/strumenti/genera_k012.py).
# Nessun dato vero: nomi, luoghi e turni sono inventati.
import json, shutil, sys, os, datetime, copy
SRC, DST = sys.argv[1], sys.argv[2]
if os.path.exists(DST): shutil.rmtree(DST)
shutil.copytree(SRC, DST)
def load(n): return json.load(open(f'{DST}/{n}.json', encoding='utf-8'))
def save(n, d):
    with open(f'{DST}/{n}.json', 'w', encoding='utf-8') as f:
        json.dump(d, f, ensure_ascii=False, indent=2); f.write('\n')

pk = load('pacchetto')
pk['codice'] = 'K011-ORARI'
pk['nome'] = "Prova degli orari di lavoro (K-011)"
pk['origine']['nota'] = ("Dati inventati per le prove degli orari di lavoro (K-011), dal pacchetto demo: i tre esempi della pausa di Mauro, "
                        "una violazione per tipo di regola, la corsa di 52 km con mezzi da 17, 8 e 9 posti, una corsa senza km, un titolare.")
save('pacchetto', pk)

# Luoghi inventati per gli esempi della pausa.
lu = load('luoghi')
base = [x for x in lu['luoghi'] if x['codice'] == 'F-STAZIONE'][0]
for i, (codice, nome, dlat) in enumerate([('F-PAUSA-A', 'Valfonda, Ponte Vecchio', 0.004), ('F-PAUSA-B', 'Valfonda, Molino', 0.008),
                                          ('F-PAUSA-C', 'Valfonda, Cave', 0.012)]):
    x = {"codice": codice, "tipo": "fermata", "nome": nome, "comune": "Valfonda",
         "coordinate": {"lat": round(base['coordinate']['lat'] + dlat, 4), "lon": base['coordinate']['lon']}}
    lu['luoghi'].append(x)
# K-011 aggiunta 2: tutti i campi personali e particolari degli schemi compaiono in almeno un pacchetto delle prove (la prova
# Unione.ComandiConPermessiParziali lo controlla): qui i codici esterni e la nota di un luogo privato.
lu['luoghi_privati'][0]['codici_esterni'] = [{"sistema": "Consorzio Valle Alta", "tipo": "scheda", "codice": "PP-0001"}]
lu['luoghi_privati'][0]['nota'] = "Campanello a sinistra del portone (prova inventata)."
save('luoghi', lu)

# Mezzi della regola dei 50 km: 17 posti, 8 posti (9 persone con il conducente: niente 561), 9 posti (10 persone: 561).
me = load('mezzi')
proto = [x for x in me['mezzi'] if x['codice'] == 'ALFA-01'][0]
for codice, cat, posti, targa in [('V-17', 'M2', 17, 'AA 917 AA'), ('V-8', 'M1', 8, 'AA 908 AA'), ('V-9', 'M2', 9, 'AA 909 AA')]:
    x = copy.deepcopy(proto)
    x.update({"codice": codice, "uso": "LINEA", "targa": targa, "categoria": cat, "posti": posti, "posti_in_piedi": 0})
    x.pop('autorizzazioni_uso', None)
    me['mezzi'].append(x)
save('mezzi', me)

def corsa(codice, servizio, cal, da, dalle, a, alle, km=None, nome=None):
    c = {"codice": codice, "nome": nome or codice, "servizi": [servizio], "calendario": cal,
         "passaggi": [{"luogo": da, "orario": dalle}, {"luogo": a, "orario": alle}]}
    if km is not None:
        c["passaggi"][1]["km"] = {"pieni": km}
    return c

co = load('corse')
co['corse'] += [
    corsa('C-PAUSA-A1', 'S-LIN-B610', 'CAL-FERIALI', 'F-STAZIONE', '09:30', 'F-PAUSA-A', '10:00', 10.0, "Esempio a, andata"),
    corsa('C-PAUSA-A2', 'S-LIN-B610', 'CAL-FERIALI', 'F-PAUSA-A', '11:10', 'F-STAZIONE', '11:40', 10.0, "Esempio a, ritorno"),
    corsa('C-PAUSA-B1', 'S-LIN-B610', 'CAL-FERIALI', 'F-STAZIONE', '18:30', 'F-PAUSA-B', '19:05', 20.0, "Esempio b, andata"),
    corsa('C-PAUSA-B2', 'S-LIN-B610', 'CAL-FERIALI', 'F-PAUSA-B', '19:50', 'F-STAZIONE', '20:20', 20.0, "Esempio b, ritorno"),
    corsa('C-PAUSA-C1', 'S-LIN-B610', 'CAL-FERIALI', 'F-STAZIONE', '08:30', 'F-PAUSA-C', '09:08', 12.0, "Esempio c, andata"),
    corsa('C-PAUSA-C2', 'S-LIN-B610', 'CAL-FERIALI', 'F-PAUSA-C', '12:00', 'F-STAZIONE', '12:30', 12.0, "Esempio c, ritorno"),
    corsa('C-52KM', 'S-LIN-B610', 'CAL-MER-FER', 'F-STAZIONE', '04:00', 'F-MONTEBRINO', '05:10', 52.0, "Linea di 52 km, andata"),
    corsa('C-52KM-R', 'S-LIN-B610', 'CAL-MER-FER', 'F-MONTEBRINO', '05:20', 'F-STAZIONE', '06:30', 52.0, "Linea di 52 km, ritorno"),
    corsa('C-SENZA-KM', 'S-LIN-B610', 'CAL-FERIALI', 'F-STAZIONE', '10:00', 'F-MONTEBRINO', '11:30', None, "Linea senza km"),
]
# Corse scolastiche una dopo l'altra (45 minuti di guida, 3 minuti di sosta): 5 ore di guida di fila.
for k in range(6):
    t0 = 7 * 60 + 50 + 48 * k
    da, a = ('F-STAZIONE', 'F-PRADELLO') if k % 2 == 0 else ('F-PRADELLO', 'F-STAZIONE')
    co['corse'].append(corsa(f'C-SCUOLA-LUNGA-{k + 1}', 'S-SUP-RM', 'CAL-T-SCUOLA-LUNGA', da, f'{t0 // 60:02d}:{t0 % 60:02d}', a,
                             f'{(t0 + 45) // 60:02d}:{(t0 + 45) % 60:02d}', 10.0, f"Scuola, giro {k + 1}"))
# Corse di linea ogni ora (50 minuti di guida e 10 di sosta), dalla stazione a Pradello e ritorno.
for h in range(5, 19):
    da, a = ('F-STAZIONE', 'F-PRADELLO') if h % 2 else ('F-PRADELLO', 'F-STAZIONE')
    co['corse'].append(corsa(f'C-ORA-{h:02d}', 'S-LIN-B610', 'CAL-FERIALI', da, f'{h:02d}:00', a, f'{h:02d}:50', 15.1, f"Linea delle {h:02d}"))
# K-011 aggiunta 1: il limite dei 50 km (Reg. 561 art. 3 a: "fino a 50 km" esclusi; 50,1 km no). Ogni corsa ha il
# calendario del suo turno (un giorno solo).
co['corse'] += [
    corsa('C-50KM', 'S-LIN-B610', 'CAL-T-50KM', 'F-STAZIONE', '04:00', 'F-MONTEBRINO', '05:10', 50.0, "Linea di 50 km esatti"),
    corsa('C-50KM1', 'S-LIN-B610', 'CAL-T-50KM1', 'F-STAZIONE', '04:00', 'F-MONTEBRINO', '05:10', 50.1, "Linea di 50,1 km"),
]
# K-011 aggiunta 1, OUT o tachigrafo. Servizi solo per disabili fino a 50 km con il mezzo da 17 posti: il 561 resta da
# confermare (S-DIS: avvisi); per S-DIS-GIO Alfa dichiara il servizio escluso (una nota, nessun avviso da confermare).
for sigla, servizio in [('DIS', 'S-DIS'), ('DICH', 'S-DIS-GIO')]:
    for k in range(6):
        t0 = 7 * 60 + 50 + 48 * k
        da, a = ('F-STAZIONE', 'F-PRADELLO') if k % 2 == 0 else ('F-PRADELLO', 'F-STAZIONE')
        co['corse'].append(corsa(f'C-{sigla}-LUNGA-{k + 1}', servizio, f'CAL-T-{sigla}-LUNGA', da, f'{t0 // 60:02d}:{t0 % 60:02d}', a,
                                 f'{(t0 + 45) // 60:02d}:{(t0 + 45) % 60:02d}', 10.0, f"Disabili, giro {k + 1}"))
# Forzatura: una linea di 55 km con i km scritti, che per Alfa e' di 48 km (il percorso vero): OUT forzato.
co['corse'].append(corsa('C-FORZATA', 'S-LIN-B610', 'CAL-T-FORZATA', 'F-STAZIONE', '06:00', 'F-MONTEBRINO', '10:20', 55.0,
                         "Linea lunga, OUT forzato dall'azienda"))
# K-011 aggiunta 2: una forzatura su un servizio e una dichiarazione su una corsa che non servono (linee fino a 50 km, gia' fuori
# dal Reg. 561), una giornata sola.
co['corse'] += [
    corsa('C-NONSERVE-1', 'S-LIN-PROVA', 'CAL-T-NONSERVE', 'F-STAZIONE', '08:00', 'F-PRADELLO', '08:30', 10.0, "Linea di prova, andata"),
    corsa('C-NONSERVE-2', 'S-LIN-B610', 'CAL-T-NONSERVE', 'F-PRADELLO', '08:40', 'F-STAZIONE', '09:10', 10.0, "Linea, ritorno"),
]
save('corse', co)
se = load('servizi')
lin = copy.deepcopy([x for x in se['servizi'] if x['codice'] == 'S-LIN-B610'][0])
lin.update({"codice": "S-LIN-PROVA", "nome": "Linea di prova della forzatura che non serve"})
lin.pop('codici_esterni', None)
lin['domanda']['gruppi'][0]['codice'] = 'G-LIN-PROVA'
# Il servizio in fondo all'elenco, senza riscrivere il resto del file (resta come nel demo).
testo = open(f'{DST}/servizi.json', encoding='utf-8').read()
coda = testo.rstrip().rsplit(']', 1)
assert len(coda) == 2 and coda[1].strip() == '}'
nuovo = json.dumps(lin, ensure_ascii=False, indent=2).replace('\n', '\n    ')
with open(f'{DST}/servizi.json', 'w', encoding='utf-8') as f:
    f.write(coda[0].rstrip() + ',\n    ' + nuovo + '\n  ]\n}\n')

ca = load('calendari')
tu = {"schema": "turni", "schema_versione": 1, "turni": []}
sf = {"schema": "servizi_fissi", "schema_versione": 1, "servizi_fissi": []}
GIORNI = ["lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"]

def turno(codice, nome, persona, data, elementi, mezzo='ALFA-05', azienda='ALFA', ruolo='autista', inizio=None, fine=None):
    cal = f'CAL-{codice}'
    ca['calendari'].append({"codice": cal, "nome": f"Solo il {data}", "dal": data, "al": data, "giorni": GIORNI})
    rimessa = 'L-RIMESSA-BETA' if azienda == 'BETA' else 'L-RIMESSA-ALFA'
    t = {"codice": codice, "nome": nome, "azienda": azienda, "ruolo": ruolo, "calendario": cal,
         "mezzo": {"modo": "preciso", "codice": mezzo} if mezzo else {"modo": "nessuno"}, "inizio": inizio or rimessa,
         "fine": fine or rimessa, "elementi": elementi}
    tu['turni'].append(t)
    sf['servizi_fissi'].append({"codice": f'SF-{codice}', "turno": codice, "persona": persona, "calendario": cal})

def att(tipo, luogo, dalle, alle, verso=None, km=None):
    a = {"attivita": tipo, "luogo": luogo, "dalle": dalle, "alle": alle}
    if verso: a["verso"] = verso
    if km is not None: a["km_vuoti"] = km
    return a
def c(codice): return {"corsa": codice}
def ore(h0, h1):   # le corse di linea delle ore h0 ... h1
    return [c(f'C-ORA-{h:02d}') for h in range(h0, h1 + 1)]
def apertura(dalle, alle, verso, arrivo):
    return [att('PRESA', 'L-RIMESSA-ALFA', dalle, alle), att('VUOTO', 'L-RIMESSA-ALFA', alle, arrivo, verso, 1.0)]
def chiusura(da, dalle, alle):
    h, m = map(int, alle.split(':'))
    fine = f'{h:02d}:{m + 10:02d}' if m < 50 else f'{h + 1:02d}:{m - 50:02d}'
    return [att('VUOTO', da, dalle, alle, 'L-RIMESSA-ALFA', 1.0), att('FINE', 'L-RIMESSA-ALFA', alle, fine)]
def dopo(orario, minuti):
    h, m = map(int, orario.split(':')); t = h * 60 + m + minuti
    return f'{t // 60:02d}:{t % 60:02d}'
def fine_ore(h1):   # dove e quando finisce la corsa dell'ora h1
    return ('F-PRADELLO' if h1 % 2 else 'F-STAZIONE'), f'{h1:02d}:50'
def giro_ore(h0, h1, presa=None):
    p = presa or dopo(f'{h0:02d}:00', -25)
    inizio = 'F-STAZIONE' if h0 % 2 else 'F-PRADELLO'
    luogo, alle = fine_ore(h1)
    return apertura(p, dopo(p, 10), inizio, dopo(p, 20)) + ore(h0, h1) + chiusura(luogo, alle, dopo(alle, 10))

# Esempi della pausa (Mauro, 20/10): (a) 20 + 20 minuti di guida per 30 minuti fermo; (b) 19:05 -> 19:50 dallo stesso luogo,
# 20 km e 15 minuti per tratta, 15 minuti fermo; (c) 9:08 -> 12:00 dallo stesso luogo, 12 km e 18 minuti per tratta.
turno('T-PAUSA-A', "Esempio a della pausa", 'P-PAUSE', '2026-11-02',
      apertura('09:00', '09:10', 'F-STAZIONE', '09:25') + [c('C-PAUSA-A1'),
      att('VUOTO', 'F-PAUSA-A', '10:00', '10:20', 'L-RIMESSA-ALFA', 10.0), att('PAUSA', 'L-RIMESSA-ALFA', '10:20', '10:50'),
      att('VUOTO', 'L-RIMESSA-ALFA', '10:50', '11:10', 'F-PAUSA-A', 10.0), c('C-PAUSA-A2')] + chiusura('F-STAZIONE', '11:40', '11:50'))
turno('T-PAUSA-B', "Esempio b della pausa", 'P-PAUSE', '2026-11-03',
      apertura('18:00', '18:10', 'F-STAZIONE', '18:25') + [c('C-PAUSA-B1'),
      att('VUOTO', 'F-PAUSA-B', '19:05', '19:20', 'L-RIMESSA-ALFA', 20.0), att('PAUSA', 'L-RIMESSA-ALFA', '19:20', '19:35'),
      att('VUOTO', 'L-RIMESSA-ALFA', '19:35', '19:50', 'F-PAUSA-B', 20.0), c('C-PAUSA-B2')] + chiusura('F-STAZIONE', '20:20', '20:30'))
turno('T-PAUSA-C', "Esempio c della pausa", 'P-PAUSE', '2026-11-04',
      apertura('07:50', '08:00', 'F-STAZIONE', '08:15') + [c('C-PAUSA-C1'),
      att('VUOTO', 'F-PAUSA-C', '09:08', '09:26', 'L-RIMESSA-ALFA', 12.0), att('PAUSA', 'L-RIMESSA-ALFA', '09:26', '11:42'),
      att('VUOTO', 'L-RIMESSA-ALFA', '11:42', '12:00', 'F-PAUSA-C', 12.0), c('C-PAUSA-C2')] + chiusura('F-STAZIONE', '12:30', '12:40'))

# L. 138 certa (dipendente, linea fino a 50 km): giovedi' 5 novembre 11 ore di linea con soste di 10 minuti (lavoro
# effettivo oltre 10 ore, guida di fila oltre 5 ore); venerdi' si riprende all'01:00 (riposo sotto le 9 ore).
turno('T-L138-GIO', "Giornata lunga di linea", 'P-L138', '2026-11-05', giro_ore(6, 16))
turno('T-L138-VEN', "Riserva di notte", 'P-L138', '2026-11-06', [att('RISERVA', 'L-RIMESSA-ALFA', '01:00', '03:00')], mezzo=None)

# Regola dei 50 km: la corsa di 52 km di mercoledi' con il mezzo da 17 posti porta il 561 (e il 234) su tutta la settimana.
def giro_52(presa='03:30'):
    return (apertura(presa, dopo(presa, 15), 'F-STAZIONE', dopo(presa, 30)) + [c('C-52KM'), c('C-52KM-R')])
for data in ['2026-11-09', '2026-11-10', '2026-11-13', '2026-11-14']:
    turno(f'T-561-{data[8:]}', "Linea corta", 'P-561-17', data, giro_ore(6, 9), mezzo='V-17')
turno('T-561-11', "Linea di 52 km e linea corta, con lavoro di notte", 'P-561-17', '2026-11-11',
      giro_52() + ore(7, 15) + chiusura('F-PRADELLO', '15:50', '16:00'), mezzo='V-17')
turno('T-561-12', "Linea tutto il giorno", 'P-561-17', '2026-11-12', giro_ore(5, 17, '04:35'), mezzo='V-17')
turno('T-561-15', "Gita della domenica", 'P-561-17', '2026-11-15',
      [att('PRESA', 'L-RIMESSA-ALFA', '07:45', '08:00'), c('C-GITA-A'), att('ATTESA', 'F-LAGOBELLO', '08:45', '17:00'), c('C-GITA-R'),
       att('FINE', 'L-RIMESSA-ALFA', '17:45', '18:00')], mezzo='ALFA-01')
turno('T-52KM-8', "Linea di 52 km con il mezzo da 8 posti", 'P-561-8', '2026-11-18',
      giro_52() + chiusura('F-STAZIONE', '06:30', '06:40'), mezzo='V-8')
turno('T-52KM-9', "Linea di 52 km con il mezzo da 9 posti", 'P-561-9', '2026-11-25',
      giro_52() + chiusura('F-STAZIONE', '06:30', '06:40'), mezzo='V-9')
turno('T-SENZA-KM', "Linea senza km", 'P-SENZA-KM', '2026-11-19',
      apertura('09:30', '09:40', 'F-STAZIONE', '09:55') + [c('C-SENZA-KM')] +
      [att('VUOTO', 'F-MONTEBRINO', '11:30', '12:00', 'L-RIMESSA-ALFA', 14.0), att('FINE', 'L-RIMESSA-ALFA', '12:00', '12:10')], mezzo='V-17')

# Titolare oltre le 8 ore della L. 138: da confermare (avviso, non errore).
turno('T-TITOLARE', "Linea del titolare", 'P-TITOLARE', '2026-11-20', giro_ore(7, 15))

# Lavoro a chiamata: 398 giornate prima del pacchetto, cinque giornate in novembre.
for g in range(23, 28):
    turno(f'T-CHIAMATA-{g}', "Linea del mattino", 'P-CHIAMATA', f'2026-11-{g}', giro_ore(7, 8))

# Contratto collettivo inventato ESEMPIO-01: otto giorni di fila (al massimo 7), poi una giornata di 14 ore e 30 (al massimo 14).
for i in range(8):
    d = (datetime.date(2026, 11, 30) + datetime.timedelta(days=i)).isoformat()
    turno(f'T-CONTRATTO-{i}', "Riserva", 'P-CONTRATTO', d, [att('RISERVA', 'L-RIMESSA-ALFA', '08:00', '09:00')], mezzo=None)
turno('T-CONTRATTO-AMPIA', "Riserva lunga", 'P-CONTRATTO', '2026-12-09',
      [att('RISERVA', 'L-RIMESSA-ALFA', '05:00', '06:00'), att('RISERVA', 'L-RIMESSA-ALFA', '18:30', '19:30')], mezzo=None)

# Ore massime del contratto della persona (4 al giorno) superate con la L. 138 certa: 5 corse di linea (errore K-CTR-119).
turno('T-ORE', "Linea oltre il contratto", 'P-ORE', '2026-12-01', giro_ore(7, 11))
# Due autisti a bordo sulla corsa di 52 km del mercoledi' con il mezzo da 17 posti (561 certo): multipresenza, che il
# programma non tratta (avviso K-CTR-108). Il secondo autista non ha il mezzo: e' quello del turno dell'autista.
turno('T-MULTI', "Secondo autista sulla linea di 52 km", 'P-MULTI', '2026-11-11', [c('C-52KM'), c('C-52KM-R')], mezzo=None,
      ruolo='secondo_autista', inizio='F-STAZIONE', fine='F-STAZIONE')
# Guida di fila oltre le 4 ore e 30 su corse scolastiche fino a 50 km: il 561 vale da confermare (avviso K-CTR-114).
turno('T-SCUOLA-LUNGA', "Scuola tutta la mattina", 'P-GUIDA', '2026-11-27',
      apertura('07:20', '07:30', 'F-STAZIONE', '07:45') + [c(f'C-SCUOLA-LUNGA-{k + 1}') for k in range(6)] +
      chiusura('F-STAZIONE', '12:35', '12:45'), mezzo='V-17')

# Chiave del rientro di Beta (fermo minimo 45 minuti, limite rigido): 30 minuti fermo in rimessa, su un turno ricorrente.
turno('T-RIENTRO-BETA', "Rientro troppo breve", 'P-RIENTRO-BETA', '2026-11-05',
      [att('PRESA-SERVIZIO', 'L-RIMESSA-BETA', '06:20', '06:35'), c('C-B620-01'), c('C-B620-02'),
       att('RIENTRO', 'F-SORGENTA', '07:50', '08:00', 'L-RIMESSA-BETA', 3.0), att('SOSTA', 'L-RIMESSA-BETA', '08:00', '08:30'),
       att('RIENTRO', 'L-RIMESSA-BETA', '08:30', '08:40', 'F-SORGENTA', 3.0), att('SOSTA', 'F-SORGENTA', '08:40', '12:30'),
       c('C-B620-03'), att('RIENTRO', 'F-SORGENTA', '13:00', '13:10', 'L-RIMESSA-BETA', 3.0)], mezzo='BETA-02', azienda='BETA')

# K-011 aggiunta 1: corsa di linea di esattamente 50 km (esclusa dal 561) e di 50,1 km (561 certo), con il mezzo da 17 posti.
turno('T-50KM', "Linea di 50 km esatti", 'P-50KM', '2026-11-16',
      apertura('03:30', '03:45', 'F-STAZIONE', '04:00') + [c('C-50KM')] + chiusura('F-MONTEBRINO', '05:10', '05:40'), mezzo='V-17')
turno('T-50KM1', "Linea di 50,1 km", 'P-50KM1', '2026-11-17',
      apertura('03:30', '03:45', 'F-STAZIONE', '04:00') + [c('C-50KM1')] + chiusura('F-MONTEBRINO', '05:10', '05:40'), mezzo='V-17')

# K-011 aggiunta 1: le giornate dei servizi solo per disabili (con e senza la dichiarazione) e della forzatura.
for sigla, persona_, mezzo_ in [('DIS', 'P-DISABILI', 'V-17'), ('DICH', 'P-DICHIARA', 'V-9')]:
    turno(f'T-{sigla}-LUNGA', "Disabili tutta la mattina", persona_, '2026-11-26',
          apertura('07:20', '07:30', 'F-STAZIONE', '07:45') + [c(f'C-{sigla}-LUNGA-{k + 1}') for k in range(6)] +
          chiusura('F-STAZIONE', '12:35', '12:45'), mezzo=mezzo_)
# 15 minuti di trasferimento e 4 ore e 20 di linea: 4 ore e 35 di guida di fila, oltre le 4 ore e 30 del 561 ma dentro le 5
# ore della L. 138 (che resta sopra e vale con certezza).
turno('T-FORZATA', "Linea lunga in OUT forzato", 'P-FORZATA', '2026-11-24',
      apertura('05:30', '05:45', 'F-STAZIONE', '06:00') + [c('C-FORZATA'), att('FINE', 'F-MONTEBRINO', '10:20', '10:30')], mezzo='V-17',
      fine='F-MONTEBRINO')

# K-011 aggiunta 2: la forzatura e la dichiarazione che non servono.
turno('T-NONSERVE', "Linea con forzatura e dichiarazione che non servono", 'P-NONSERVE', '2026-11-23',
      apertura('07:30', '07:45', 'F-STAZIONE', '08:00') + [c('C-NONSERVE-1'), c('C-NONSERVE-2')] +
      chiusura('F-STAZIONE', '09:10', '09:20'), mezzo='V-17')

save('calendari', ca)
save('turni', tu)
save('servizi_fissi', sf)
save('rotazioni', {"schema": "rotazioni", "schema_versione": 1, "rotazioni": []})
save('assegnazioni', {"schema": "assegnazioni", "schema_versione": 1, "assegnazioni": []})
save('blocchi', {"schema": "blocchi", "schema_versione": 1, "blocchi": []})
# K-011 aggiunta 2: un'assenza di qualche ora (dalle, alle), in un giorno senza turni.
save('assenze', {"schema": "assenze", "schema_versione": 1, "assenze": [
    {"persona": "P-NONSERVE", "tipo": "PERMESSO", "dal": "2026-11-30", "al": "2026-11-30", "dalle": "10:00", "alle": "12:00"}]})
save('fermi', {"schema": "fermi", "schema_versione": 1, "fermi": []})

pe = load('persone')
pa = [x for x in pe['persone'] if x['codice'] == 'P-A'][0]
pb = [x for x in pe['persone'] if x['codice'] == 'P-B1'][0]
def persona(codice, nome, rapporto, da=pa, **extra):
    x = {"codice": codice, "azienda": da['azienda'], "ruoli": ["autista"], "nome": nome, "presa_servizio": da['presa_servizio'],
         "rapporto": {"tipo": rapporto}, "abilitazioni": da['abilitazioni']}
    for k in ['autorizzazioni', 'competenze']:
        if k in da: x[k] = da[k]
    x.update(extra)
    return x
pe['persone'] += [
    persona('P-PAUSE', "Esempi Pausa", 'dipendente_tempo_pieno'),
    persona('P-L138', "Linea Lunga", 'dipendente_tempo_pieno'),
    persona('P-561-17', "Mezzo Diciassette", 'dipendente_tempo_pieno'),
    persona('P-561-8', "Mezzo Otto", 'dipendente_tempo_pieno'),
    persona('P-561-9', "Mezzo Nove", 'dipendente_tempo_pieno'),
    persona('P-SENZA-KM', "Senza Km", 'dipendente_tempo_pieno'),
    persona('P-TITOLARE', "Titolare Prova", 'titolare'),
    persona('P-CHIAMATA', "Chiamata Prova", 'a_chiamata', giornate_a_chiamata={"giornate": 398, "contate_dal": "2023-09-01"}),
    persona('P-CONTRATTO', "Contratto Esempio", 'dipendente_tempo_pieno',
            contratto_collettivo={"codice": "ESEMPIO-01", "nome": "Contratto collettivo di esempio (inventato)"}),
    persona('P-RIENTRO-BETA', "Rientro Beta", 'dipendente_tempo_pieno', da=pb),
    persona('P-ORE', "Ore Contratto", 'dipendente_tempo_pieno', ore=[{"previste": {"settimana": 20}, "massime": {"giorno": 4}}]),
    persona('P-MULTI', "Secondo Autista", 'dipendente_tempo_pieno'),
    persona('P-GUIDA', "Guida Lunga", 'dipendente_tempo_pieno'),
    persona('P-50KM', "Cinquanta Esatti", 'dipendente_tempo_pieno'),
    persona('P-50KM1', "Cinquanta Virgola Uno", 'dipendente_tempo_pieno'),
    persona('P-DISABILI', "Disabili Lunga", 'dipendente_tempo_pieno'),
    persona('P-DICHIARA', "Dichiarazione Prova", 'dipendente_tempo_pieno'),
    persona('P-FORZATA', "Forzatura Prova", 'dipendente_tempo_pieno'),
    persona('P-NONSERVE', "Nonserve Prova", 'dipendente_tempo_pieno', nota="Preferisce i turni del mattino (prova inventata)."),
]
save('persone', pe)

# K-011 aggiunta 1: la dichiarazione e la forzatura di Alfa (motivo e chi sono dati personali, inventati).
az = load('aziende')
alfa = [x for x in az['aziende'] if x['azienda'] == 'ALFA'][0]
alfa['modo_corse']['servizi'] = [
    {"servizio": "S-DIS-GIO", "modo": "out",
     "dichiarazione": {"motivo": "Il centro diurno del giovedi' e' per l'azienda un servizio scolastico speciale (prova inventata).",
                       "chi": "Ufficio turni Alfa", "quando": "2026-10-28"}}]
alfa['modo_corse']['corse'] = [
    {"corsa": "C-FORZATA", "modo": "out",
     "forzatura": {"motivo": "Il percorso vero e' di 48 km: i km scritti contano un tratto che la linea non fa piu' (prova inventata).",
                   "chi": "Ufficio turni Alfa", "quando": "2026-10-28", "fino_al": "2027-06-30"}},
    # K-011 aggiunta 2: una dichiarazione su una corsa di linea, dove l'esclusione e' gia' certa (non serve).
    {"corsa": "C-NONSERVE-2", "modo": "out",
     "dichiarazione": {"motivo": "Dichiarazione scritta per prova su una linea corta (prova inventata).",
                       "chi": "Ufficio turni Alfa", "quando": "2026-10-28"}}]
# K-011 aggiunta 2: una forzatura su un servizio di linea fino a 50 km, gia' fuori dal Reg. 561 (non serve).
alfa['modo_corse']['servizi'].append(
    {"servizio": "S-LIN-PROVA", "modo": "out",
     "forzatura": {"motivo": "Forzatura scritta per prova su una linea corta (prova inventata).",
                   "chi": "Ufficio turni Alfa", "quando": "2026-10-28"}})
save('aziende', az)
