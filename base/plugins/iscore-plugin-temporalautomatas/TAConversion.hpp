#pragma once
#include <QString>
#include <sstream>
#include <Process/TimeValue.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <set>
#include <eggs/variant.hpp>
class AutomationModel;
class TimeNodeModel;
class EventModel;
class ConstraintModel;
class StateModel;
namespace Scenario
{ class ScenarioModel; }
namespace TA
{ struct TAScenario; }

template<typename Object>
QString name(const Object& obj)
{
    ObjectPath path = Path<Object>{obj}.unsafePath();
    path.vec().erase(path.vec().begin(), path.vec().begin() + 3);
    std::reverse(path.vec().begin(), path.vec().end());
    for(ObjectIdentifier& elt : path.vec())
    {
        if(elt.objectName() == "Scenario")
            elt = ObjectIdentifier{"S", elt.id()};
        else if(elt.objectName() == "EventModel")
            elt = ObjectIdentifier{"E", elt.id()};
        else if(elt.objectName() == "ConstraintModel")
            elt = ObjectIdentifier{"C", elt.id()};
        else if(elt.objectName() == "BaseConstraintModel")
            elt = ObjectIdentifier{"B", elt.id()};
        else if(elt.objectName() == "TimeNodeModel")
            elt = ObjectIdentifier{"T", elt.id()};
    }

    return path.toString().replace('/', "_").replace('.', "").prepend('_');
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
    Point(const QString& name):
        name{name}
    {

    }

    QString name;
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

using Constraint = eggs::variant<Rigid, Flexible>;

struct ScenarioContent
{
        std::set<TA::BroadcastChan> broadcasts;
        std::list<TA::Rigid> rigids;
        std::list<TA::Flexible> flexibles;
        std::list<TA::Point> points;
        std::list<TA::Event> events;
};


struct TAScenario : public ScenarioContent
{
    template<typename T>
    TAScenario(const Scenario::ScenarioModel& s, const T& constraint):
        iscore_scenario{s},
        self{constraint},
        event_s{constraint.event_s},
        skip{constraint.skip},
        kill{constraint.kill}
    {
        broadcasts.insert(event_s);
        broadcasts.insert(skip);
        broadcasts.insert(kill);
    }

    const Scenario::ScenarioModel& iscore_scenario;

    TA::Constraint self; // The scenario is considered similar to a constraint.

    const TA::BroadcastChan event_s;// = "skip_S" + name(iscore_scenario);
    const TA::BroadcastChan skip;// = "skip_S" + name(iscore_scenario);
    const TA::BroadcastChan kill;// = "kill_S" + name(iscore_scenario);
};

}

struct TAVisitor
{
    TA::TAScenario scenario;
    template<typename T>
    TAVisitor(const Scenario::ScenarioModel& s, const T& constraint):
        scenario{s, constraint}
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
QString makeScenario(const ConstraintModel& s);
}
