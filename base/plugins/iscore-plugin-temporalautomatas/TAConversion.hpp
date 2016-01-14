#pragma once
#include <QString>
#include <sstream>
#include <Process/TimeValue.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <set>
#include <eggs/variant.hpp>

namespace Scenario
{
class TimeNodeModel;
class EventModel;
class ConstraintModel;
class StateModel;
class ScenarioModel;
}
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

using BroadcastVariable = QString;
using BoolVariable = QString;
using IntVariable = QString;
struct Event
{
    Event(QString n, IntVariable a, BroadcastVariable b, TimeValue c, int d):
        name{n},
        message{a},
        event{b},
        date{c},
        val{d}
    {

    }

    QString name;
    IntVariable message{};
    BroadcastVariable event{};
    TimeValue date{};
    int val{};
};
using Event_ND = Event;

struct Mix
{
        Mix(QString name,
            BroadcastVariable a,
            BroadcastVariable b,
            BroadcastVariable c,
            BroadcastVariable d):
            name{name},
            event_in{a},
            event_out{b},
            skip_p{c},
            kill_p{d}
        {

        }

        QString name;
        BroadcastVariable event_in;
        BroadcastVariable event_out;
        BroadcastVariable skip_p;
        BroadcastVariable kill_p;
};

struct Control
{
    Control(QString name,
            int num,
            BroadcastVariable a,
            BroadcastVariable b,
            BroadcastVariable c,
            BroadcastVariable d,
            BroadcastVariable e,
            BroadcastVariable f
            ):
        name{name},
        num_prev_rels{num},
        event_s1{a},
        skip_p{b},
        skip{c},
        event_e{d},
        kill_p{e},
        event_s2{f}
    {

    }

    QString name;
    int num_prev_rels = 0;
    BroadcastVariable event_s1;
    BroadcastVariable skip_p;
    BroadcastVariable skip;
    BroadcastVariable event_e;
    BroadcastVariable kill_p;
    BroadcastVariable event_s2;
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
    BoolVariable en; // Enabled
    IntVariable conditionMessage;

    BroadcastVariable event;

    bool urgent = true;

    BroadcastVariable event_s;
    BroadcastVariable skip_p;
    BroadcastVariable event_e;
    BroadcastVariable kill_p;

    BroadcastVariable skip;
    BroadcastVariable event_t;
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
    bool finite = true;

    BroadcastVariable event_s;
    BroadcastVariable event_min;
    BroadcastVariable event_i;
    BroadcastVariable event_max;

    BroadcastVariable skip_p;
    BroadcastVariable kill_p;
    BroadcastVariable skip;
    BroadcastVariable kill;

    QString comment;
};

struct Rigid
{
    Rigid(const QString& name):
        name{name}
    {

    }

    QString name;

    TimeValue dur;

    BroadcastVariable event_s;
    BroadcastVariable event_e1;

    BroadcastVariable skip_p;
    BroadcastVariable kill_p;
    BroadcastVariable skip;
    BroadcastVariable kill;

    BroadcastVariable event_e2;

    QString comment;
};

using Constraint = eggs::variant<Rigid, Flexible>;

struct ScenarioContent
{
        std::set<TA::BroadcastVariable> broadcasts;
        std::set<TA::IntVariable> ints;
        std::set<TA::BoolVariable> bools;

        std::list<TA::Rigid> rigids;
        std::list<TA::Flexible> flexibles;
        std::list<TA::Point> points;
        std::list<TA::Event> events;
        std::list<TA::Event_ND> events_nd;
        std::list<TA::Mix> mixs;
        std::list<TA::Control> controls;
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

    const TA::BroadcastVariable event_s;// = "skip_S" + name(iscore_scenario);
    const TA::BroadcastVariable skip;// = "skip_S" + name(iscore_scenario);
    const TA::BroadcastVariable kill;// = "kill_S" + name(iscore_scenario);
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
            const Scenario::TimeNodeModel& timenode);
    void visit(
            const Scenario::EventModel& event);
    void visit(
            const Scenario::ConstraintModel& c);
    void visit(
            const Scenario::StateModel& state);
    void visit(
            const Scenario::ScenarioModel& s);
};
namespace TA
{
QString makeScenario(const Scenario::ConstraintModel& s);
}
