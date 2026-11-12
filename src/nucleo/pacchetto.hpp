#pragma once
// K-Zero - strumenti sul pacchetto dati: indice dei codici, calendari, persone a bordo, categoria delle corse.

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain_models.hpp"

namespace kzero {

// Trova gli elementi del pacchetto per codice. Con un codice doppio vale il primo (il doppione e' un errore
// che segnalano i controlli). Il pacchetto deve restare in vita finche' si usa l'indice.
class Index {
public:
    explicit Index(const Package& p);

    const Package& package() const { return *package_; }
    const Company* company(const Code& c) const { return find(companies_, c); }
    // Impostazioni ed elenchi di un'azienda (aziende.json), se ci sono.
    const CompanySettings* settings(const Code& company) const { return find(settings_, company); }
    const Norms* norms(const Code& c) const { return find(norms_, c); }
    // Norme del paese che valgono per un'azienda, se indicate.
    const Norms* norms_of(const Code& company) const;
    const Place* place(const Code& c) const { return find(places_, c); }
    const RoadSegment* segment(const Code& c) const { return find(segments_, c); }
    const Regulation* regulation(const Code& c) const { return find(regulations_, c); }
    const HolidayCalendar* holidays(const Code& c) const { return find(holidays_, c); }
    const Calendar* calendar(const Code& c) const { return find(calendars_, c); }
    const School* school(const Code& c) const { return find(schools_, c); }
    const Client* client(const Code& c) const { return find(clients_, c); }
    const Contract* contract(const Code& c) const { return find(contracts_, c); }
    const ServiceLot* lot(const Code& c) const { return find(lots_, c); }
    const Service* service(const Code& c) const { return find(services_, c); }
    const PassengerGroup* group(const Code& c) const { return find(groups_, c); }
    const Service* service_of_group(const Code& c) const { return find(group_service_, c); }
    const Passenger* passenger(const Code& c) const { return find(passengers_, c); }
    const PassengerGroup* group_of_passenger(const Code& c) const { return find(passenger_group_, c); }
    const Trip* trip(const Code& c) const { return find(trips_, c); }
    const Vehicle* vehicle(const Code& c) const { return find(vehicles_, c); }
    const Depot* depot(const Code& c) const { return find(depots_, c); }
    const Person* person(const Code& c) const { return find(persons_, c); }
    const Shift* shift(const Code& c) const { return find(shifts_, c); }
    const Rotation* rotation(const Code& c) const { return find(rotations_, c); }
    const FixedAssignment* fixed(const Code& c) const { return find(fixed_, c); }
    const Block* block(const Code& c) const { return find(blocks_, c); }

    // Regolamento che vale per un servizio (quello del suo contratto), se esiste.
    const Regulation* regulation_of(const Service& s) const;

    // Categoria della corsa (vedi derive_service).
    TripCategory category(const Trip& t) const;   // K-017: non nota se un tipo che serve non e' noto

private:
    template <class T>
    using Map = std::unordered_map<Code, const T*>;

    template <class T>
    static const T* find(const Map<T>& m, const Code& c) {
        const auto it = m.find(c);
        return it == m.end() ? nullptr : it->second;
    }
    template <class T>
    static void fill(Map<T>& m, const std::vector<T>& v) {
        for (const T& x : v) m.emplace(x.code, &x);
    }

    const Package* package_;
    Map<Company> companies_;
    Map<CompanySettings> settings_;
    Map<Norms> norms_;
    Map<Place> places_;
    Map<RoadSegment> segments_;
    Map<Regulation> regulations_;
    Map<HolidayCalendar> holidays_;
    Map<Calendar> calendars_;
    Map<School> schools_;
    Map<Client> clients_;
    Map<Contract> contracts_;
    Map<ServiceLot> lots_;
    Map<Service> services_;
    Map<PassengerGroup> groups_;
    Map<Service> group_service_;
    Map<Passenger> passengers_;
    Map<PassengerGroup> passenger_group_;
    Map<Trip> trips_;
    Map<Vehicle> vehicles_;
    Map<Depot> depots_;
    Map<Person> persons_;
    Map<Shift> shifts_;
    Map<Rotation> rotations_;
    Map<FixedAssignment> fixed_;
    Map<Block> blocks_;
};

// Il calendario vale nel giorno d? (Regola completa accanto a Calendar, in domain_models.hpp.)
// Le date tolte e le sospensioni prevalgono su tutto; poi le date aggiunte; poi i giorni dei calendari tolti,
// poi quelli dei calendari aggiunti; infine i giorni della settimana e il calendario di base.
// Un calendario o una festivita' inesistente, o un giro di calendari che si basano o si combinano con se stessi
// (errori segnalati dai controlli), fanno valere "nessun giorno" per la parte che manca.
bool calendar_active(const Calendar& c, Day d, const Index& index);

// Primo giorno in cui il calendario vale dentro un periodo, se c'e'.
std::optional<Day> first_active_day(const Calendar& c, DateRange within, const Index& index);

// Giorni in cui valgono i calendari dentro un periodo, calcolati una volta sola: servono ai controlli e ai calcoli
// che guardano molti giorni (turni, rotazioni). Fuori dal periodo e per un calendario inesistente: nessun giorno.
class CalendarDays {
public:
    CalendarDays(const Index& index, DateRange period);

    const DateRange& period() const { return period_; }
    bool in_period(Day d) const { return d >= period_.from && d <= period_.to; }
    bool active(const Code& calendar, Day d) const;
    // Quanti giorni del periodo vale il calendario.
    int count(const Code& calendar) const;
    // I giorni del periodo di un calendario (1 = vale), per chi li guarda molte volte senza cercare il codice ogni
    // volta; nullptr se il calendario non esiste. Restano validi finche' vive questo oggetto.
    const std::vector<char>* days_of(const Code& calendar) const;
    bool active(const std::vector<char>* days, Day d) const {
        return days && in_period(d) && (*days)[static_cast<std::size_t>(d - period_.from)] != 0;
    }

private:
    DateRange period_;
    std::unordered_map<Code, std::vector<char>> days_;
};

// Persone a bordo di una corsa, gruppo per gruppo, dopo ogni passaggio.
// Non contano i gruppi con conteggi non noti; l'assistente non e' un passeggero.
struct OnBoardAfter {
    int total = 0;                     // tutte le persone dei gruppi con conteggi noti
    std::map<Code, int> by_group;      // in ordine di codice
};
std::vector<OnBoardAfter> on_board(const Trip& t, const Index& index);

}  // namespace kzero
