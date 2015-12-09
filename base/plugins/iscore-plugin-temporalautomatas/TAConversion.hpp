#pragma once
#include <QString>
#include <sstream>
#include <Process/TimeValue.hpp>
#include <iscore/tools/ModelPath.hpp>
class AutomationModel;
class TimeNodeModel;
class EventModel;
class ConstraintModel;
class StateModel;
namespace Scenario
{ class ScenarioModel; }
namespace TA
{ class TAScenario; }

template<typename Object>
QString name(const Object& obj)
{
    ObjectPath path = Path<Object>{obj}.unsafePath();
    path.vec().erase(path.vec().begin(), path.vec().begin() + 4);
    for(ObjectIdentifier& elt : path.vec())
    {
        if(elt.objectName() == "Scenario")
            elt = ObjectIdentifier{"IScenario", elt.id()};
        else if(elt.objectName() == "EventModel")
            elt = ObjectIdentifier{"IEvent", elt.id()};
        else if(elt.objectName() == "ConstraintModel")
            elt = ObjectIdentifier{"IConstraint", elt.id()};
    }

    return path.toString().replace('/', "__").replace('.', "").prepend('_');
}

namespace TA
{

using BroadcastChan = QString;
struct Event
{
    Event(QString n, int a, BroadcastChan b, TimeValue c, int d):
        name{n},
        message{a},
        event{b},
        date{c},
        val{d}
    {

    }

    QString name;
    int message{};
    BroadcastChan event{};
    TimeValue date{};
    int val{};
};

struct Point
{
    Point(const EventModel& ev):
        iscore_event{ev}
    {

    }

    const EventModel& iscore_event;
    int condition{};
    int conditionValue{};
    BroadcastChan en; // Enabled
    BroadcastChan conditionMessage;

    BroadcastChan event;

    bool urgent{};

    BroadcastChan event_s;
    BroadcastChan skip_p;
    BroadcastChan event_e;
    BroadcastChan kill_p;

    BroadcastChan skip;
    BroadcastChan event_t;
};


struct Flexible
{
    Flexible(const QString& name):
        name{name}
    {

    }

    QString name;

    TimeValue dmin;
    TimeValue dmax;
    bool finite{};

    BroadcastChan event_s;
    BroadcastChan event_min;
    BroadcastChan event_i;
    BroadcastChan event_max;

    BroadcastChan skip_p;
    BroadcastChan kill_p;
    BroadcastChan skip;
    BroadcastChan kill;
};

struct Rigid
{
    Rigid(const QString& name):
        name{name}
    {

    }

    QString name;

    TimeValue dur;

    BroadcastChan event_s;
    BroadcastChan event_e1;

    BroadcastChan skip_p;
    BroadcastChan kill_p;
    BroadcastChan skip;
    BroadcastChan kill;

    BroadcastChan event_e2;
};

struct TAScenario
{
    TAScenario(const Scenario::ScenarioModel& s):
        iscore_scenario{s},
        self{"toto"}
    {
        // TODO setup self
        broadcasts.push_back(skip_S);
        broadcasts.push_back(kill_S);
    }

    const Scenario::ScenarioModel& iscore_scenario;

    TA::Rigid self; // The scenario is considered similar to a constraint.

    const TA::BroadcastChan skip_S = "skip_S" + name(iscore_scenario);
    const TA::BroadcastChan kill_S = "kill_S" + name(iscore_scenario);

    std::vector<TA::BroadcastChan> broadcasts;
    std::vector<TA::Rigid> rigids;
    std::vector<TA::Flexible> flexibles;
    std::vector<TA::Point> points;
};

}

struct TAVisitor
{
    TA::TAScenario scenario;
    TAVisitor(const Scenario::ScenarioModel& s):
        scenario{s}
    {
        visit(s);
    }

private:
    int depth = 0;
    const char* space() const;

    void visit(
            const AutomationModel& automation);
    void visit(
            const TimeNodeModel& timenode);
    void visit(
            const EventModel& event);
    void visit(
            const ConstraintModel& c);
    void visit(
            const StateModel& state);
    void visit(
            const Scenario::ScenarioModel& s);
};
namespace TA
{
QString makeScenario(const Scenario::ScenarioModel& s);
}
