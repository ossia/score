#include "TAConversion.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Automation/AutomationModel.hpp>
#include <iscore/tools/std/Algorithms.hpp>

namespace TA
{
using BroadcastChan = QString;

template<typename T, typename Stream>
void print(const std::vector<T>& vec, Stream& s)
{
    for(const auto& elt : vec)
    {
        print(elt, s);
        s << "\n";
    }
    s << "\n";
}

template<typename Stream>
void print(const Point& pt, Stream& stream)
{
    QString s = QString("%1 = Point(%2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13)\n")
            .arg(name(pt.iscore_event))
            .arg(pt.condition)
            .arg(pt.conditionValue)
            .arg(pt.en)
            .arg(pt.conditionMessage)
            .arg(pt.event)
            .arg(pt.urgent)
            .arg(pt.event_s)
            .arg(pt.skip_p)
            .arg(pt.event_e)
            .arg(pt.kill_p)
            .arg(pt.skip)
            .arg(pt.event_t);

    stream << s.toLatin1().constData();
}

template<typename Stream>
void print(const Flexible& c, Stream& stream)
{
    QString s = QString("%1 = Flexible(%2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12)\n")
            .arg(c.name)
            .arg(c.dmin.msec())
            .arg(c.dmax.msec())
            .arg(c.finite)
            .arg(c.event_s)
            .arg(c.event_min)
            .arg(c.event_i)
            .arg(c.event_max)
            .arg(c.skip_p)
            .arg(c.kill_p)
            .arg(c.skip)
            .arg(c.kill);

    stream << s.toLatin1().constData();
}

template<typename Stream>
void print(const Rigid& c, Stream& stream)
{
    QString s = QString("%1 = Rigid(%2, %3, %4, %5, %6, %7, %8, %9)\n")
            .arg(c.name)
            .arg(c.dur.msec())
            .arg(c.event_s)
            .arg(c.event_e1)
            .arg(c.skip_p)
            .arg(c.kill_p)
            .arg(c.skip)
            .arg(c.kill)
            .arg(c.event_e2);

    stream << s.toLatin1().constData();
}


template<typename Stream>
void print(const Event& c, Stream& stream)
{
    QString s = QString("%1 = Event(%2, %3, %4, %5)\n")
            .arg(c.name)
            .arg(c.message)
            .arg(c.event)
            .arg(c.date.msec())
            .arg(c.val);;

    stream << s.toLatin1().constData();
}


QString makeScenario(const Scenario::ScenarioModel &s)
{
    TAVisitor v{s};
    // Then create an external event for the start event.

    TA::Event scenario_start_event{"MainStartEvent", 0, BroadcastChan{"global_start"}, TimeValue::zero(), 1};
    for(auto& element : v.scenario.points)
    {
        if(element.event_s.isEmpty())
            element.event_s = scenario_start_event.event;
    }

    std::stringstream output;

    print(scenario_start_event, output);
    print(v.scenario.rigids, output);
    print(v.scenario.flexibles, output);
    print(v.scenario.points, output);
    return QString::fromStdString(output.str());
}

}

const char* TAVisitor::space() const
{
    return qPrintable(QString(depth, '-'));
}

void TAVisitor::visit(const AutomationModel &automation)
{
    qDebug() << space() << "Visiting automation" << automation.id();
    automation.duration(); // In ms
    automation.address();
    automation.min();
    automation.max();

    for(const auto& segment : Curve::orderedSegments(automation.curve()))
    {
        //segment.start; // x, y
        //segment.end;
        //segment.previous; // id of previous segment
        //segment.following; // id of following segment
    }
}

void TAVisitor::visit(const TimeNodeModel &timenode)
{
    qDebug() << space() << "Visiting timenode" << timenode.id();
    auto trigger = timenode.trigger();
    trigger->expression();

    timenode.events();
}

void TAVisitor::visit(const EventModel &event)
{
    qDebug() << space() << "Visiting event" << event.id();
    event.states();

    TA::Point point{event};
    QString event_name = name(event);

    // TODO condition
    // TODO states

    point.kill_p = scenario.kill_S;
    point.en = "en_" + event_name;
    point.event = "event_" + event_name;
    point.skip = "skip_" + event_name;
    point.event_t = "ok_" + event_name;

    point.condition = 0;
    point.conditionMessage = "msg_true";

    point.urgent = false;
    scenario.points.push_back(point);

    scenario.broadcasts.push_back(point.en);
    scenario.broadcasts.push_back(point.event);
    scenario.broadcasts.push_back(point.skip);
    scenario.broadcasts.push_back(point.event_t);
}

void TAVisitor::visit(const StateModel &state)
{
    qDebug() << space() << "Visiting state" << state.id();
    state.previousConstraint();
    state.nextConstraint();

    iscore::MessageList messages = flatten(state.messages().rootNode());
    for(auto message : messages)
    {
        auto addr = message.address;

        auto val = message.value.val;
        switch(val.which())
        {
        case iscore::ValueType::Impulse:
        {
            break;
        }
        case iscore::ValueType::Int:
        {
            int actual_value = val.get<int>();
            break;
        }
        case iscore::ValueType::Float:
        {
            float actual_value = val.get<float>();
            break;
        }
        case iscore::ValueType::Bool:
        {
            bool actual_value = val.get<bool>();
            break;
        }
        case iscore::ValueType::String:
        {
            QString actual_value = val.get<QString>();
            break;
        }
        case iscore::ValueType::Char:
        {
            QChar actual_value = val.get<QChar>();
            break;
        }
        case iscore::ValueType::Tuple:
        {
            tuple_t actual_value = val.get<iscore::tuple_t>();
            for(auto tuple_val : actual_value)
            {
                // switch(tuple_val.val) ...
            }
            break;
        }
        case iscore::ValueType::NoValue:
        {
            break;
        }
        }
    }
}

void TAVisitor::visit(const Scenario::ScenarioModel &s)
{
    qDebug() << space() << "Visiting scenario" << s.id();
    depth++;
    const auto& eev = s.endEvent();
    const auto& eev_id = eev.id();
    const auto& etn_id = eev.timeNode();
    for(const TimeNodeModel& timenode : s.timeNodes)
    {
        if(timenode.id() != etn_id)
            visit(timenode);
    }
    for(const EventModel& event : s.events)
    {
        if(event.id() != eev_id)
            visit(event);
    }

    for(const StateModel& state : s.states)
    {
        visit(state);
    }

    for(const ConstraintModel& constraint : s.constraints)
    {
        visit(constraint);
    }
    depth--;
}

void TAVisitor::visit(const ConstraintModel &c)
{
    qDebug() << space() << "Visiting constraint" << c.id();

    QString start_event_name = name(startEvent(c, scenario.iscore_scenario));
    QString end_event_name = name(endEvent(c, scenario.iscore_scenario));

    QString cst_name = name(c);

    if(c.duration.isRigid())
    {
        TA::Rigid rigid{cst_name};
        rigid.dur = c.duration.defaultDuration();

        rigid.event_s = "event_" + start_event_name;
        rigid.event_e1 = "event_" + end_event_name;
        rigid.event_e2 = "event_" + end_event_name;

        rigid.skip_p = scenario.skip_S;
        rigid.kill_p = scenario.kill_S;

        rigid.skip = "skip_" + cst_name;
        rigid.kill = "kill_" + cst_name;

        scenario.rigids.push_back(rigid);
        scenario.broadcasts.push_back(rigid.event_e1);
        scenario.broadcasts.push_back(rigid.event_e2);
        scenario.broadcasts.push_back(rigid.skip);
        scenario.broadcasts.push_back(rigid.kill);

        auto it = find_if(scenario.points,
                          [&] (const auto& pt) { return pt.event == rigid.event_e1; });
        if(it != scenario.points.end())
        {
            TA::Point& pt = *it;
            pt.event_s = rigid.event_e1;
            pt.skip_p = rigid.skip;
            pt.event_e = rigid.event_e2;
        }
    }
    else
    {
        TA::Flexible flexible{cst_name};

        // TODO
        flexible.dmin = c.duration.minDuration();
        flexible.dmax = c.duration.maxDuration();

        flexible.event_s = "event_" + start_event_name;
        flexible.skip_p = scenario.skip_S;
        flexible.kill_p = scenario.kill_S;

        QString cst_name = name(c);
        flexible.event_min = "min_" + cst_name;
        flexible.event_max = "max_" + cst_name;
        flexible.event_i = "event_" + end_event_name;

        flexible.skip = "skip_" + cst_name;
        flexible.kill = "kill_" + cst_name;

        scenario.flexibles.push_back(flexible);
        scenario.broadcasts.push_back(flexible.event_min);
        scenario.broadcasts.push_back(flexible.event_max);
        scenario.broadcasts.push_back(flexible.skip);
        scenario.broadcasts.push_back(flexible.kill);

        auto it = find_if(scenario.points, [&] (const auto& pt) { return pt.event == flexible.event_i; });
        if(it != scenario.points.end())
        {
            TA::Point& pt = *it;
            pt.event_s = flexible.event_min;
            pt.skip_p = flexible.skip;
            pt.event_e = flexible.event_max;
        }
    }


    /*
            if(c.duration.isRigid())
            {
                c.duration.defaultDuration();
            }
            else
            {
                c.duration.minDuration();
                c.duration.maxDuration();
            }*/

    depth ++;/*
            for(const auto& process : c.processes)
            {
                if(auto autom = dynamic_cast<const AutomationModel*>(&process))
                {
                    visit(*autom);
                }
                else if(auto scenario = dynamic_cast<const Scenario::ScenarioModel*>(&process))
                {
                    visit(*scenario);
                }
            }
*/
    depth --;
}
