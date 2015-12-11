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

template<typename Container, typename Stream>
void print(const Container& vec, Stream& s)
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
            .arg(pt.name)
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
            .arg(c.finite ? c.dmax.msec() : -1)
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

QString print(const ScenarioContent& c)
{
    std::stringstream output;

    print(c.events, output);
    print(c.rigids, output);
    print(c.flexibles, output);
    print(c.points, output);

    return QString::fromStdString(output.str());
}

void insert(TA::ScenarioContent& source, TA::ScenarioContent& dest)
{
    dest.rigids.splice(dest.rigids.end(), source.rigids);
    dest.flexibles.splice(dest.flexibles.end(), source.flexibles);
    dest.points.splice(dest.points.end(), source.points);
    dest.broadcasts.insert(source.broadcasts.begin(), source.broadcasts.end());
}


template<typename T>
void visitProcesses(
        const ConstraintModel& c,
        const T& ta_cst,
        TA::ScenarioContent& content)
{
    for(const auto& process : c.processes)
    {
        if(auto scenario = dynamic_cast<const Scenario::ScenarioModel*>(&process))
        {
            TAVisitor v{*scenario, ta_cst};

            for(const TA::Point& point : v.scenario.points)
            {
                ISCORE_ASSERT(!point.event_s.isEmpty());
                ISCORE_ASSERT(!point.event_e.isEmpty());
                ISCORE_ASSERT(!point.skip_p.isEmpty());
            }

            insert(v.scenario, content);
        }
    }
}

QString makeScenario(const ConstraintModel &c)
{
    // Global play

    // Our register of elements
    ScenarioContent baseContent;

    TA::Event scenario_start_event{"MainStartEvent", 0, BroadcastChan{"global_start"}, TimeValue::zero(), 1};

    QString cst_name = name(c);

    // Setup of the rigid
    TA::Rigid rigid{cst_name};
    rigid.dur = c.duration.defaultDuration();

    rigid.event_s = scenario_start_event.event;
    rigid.event_e1 = "event_e1" + cst_name;
    rigid.event_e2 = "event_e2" + cst_name;

    rigid.skip_p = "skip_p" + cst_name;
    rigid.kill_p = "kill_p" + cst_name;

    rigid.skip = "skip" + cst_name;
    rigid.kill = "kill" + cst_name;

    // Register all the new elements
    baseContent.rigids.push_back(rigid);
    baseContent.events.push_back(scenario_start_event);
    baseContent.broadcasts.insert(rigid.event_e1);
    baseContent.broadcasts.insert(rigid.event_e2);
    baseContent.broadcasts.insert(rigid.skip);
    baseContent.broadcasts.insert(rigid.kill);

    visitProcesses(c, rigid, baseContent);

    return print(baseContent);
}

}

const char* TAVisitor::space() const
{
    return qPrintable(QString(depth, '-'));
}

void TAVisitor::visit(const AutomationModel &automation)
{
    qDebug() << space() << "Visiting automation" << automation.id();
    /*
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
    */
}

void TAVisitor::visit(const TimeNodeModel &timenode)
{
    qDebug() << space() << "Visiting timenode" << timenode.id();

    // First we create a point for the timenode. The ingoing
    // constraints will end on this point.


    QString tn_name = name(timenode);
    TA::Point tn_point{tn_name};

    // TODO trigger

    tn_point.kill_p = scenario.kill;
    tn_point.en = "en_" + tn_name;
    tn_point.event = "event_" + tn_name;
    tn_point.skip = "skip_" + tn_name;
    tn_point.event_t = "ok_" + tn_name;

    tn_point.condition = 0;
    tn_point.conditionMessage = "msg_true";

    tn_point.urgent = false;
    if(&timenode == &scenario.iscore_scenario.startTimeNode())
    {
        tn_point.skip_p = scenario.skip;
        tn_point.event_s = scenario.event_s;
        tn_point.event_e = scenario.event_s;
    }

    scenario.points.push_back(tn_point);

    scenario.broadcasts.insert(tn_point.en);
    scenario.broadcasts.insert(tn_point.event);
    scenario.broadcasts.insert(tn_point.skip);
    scenario.broadcasts.insert(tn_point.event_t);

    for(const auto& event_id : timenode.events())
    {
        // We create flexibles that goes to each event.
        const auto& event = scenario.iscore_scenario.events.at(event_id);

        QString event_name = name(event);

        QString flexible_name = tn_name + "__to__" + event_name;
        TA::Flexible flexible{flexible_name};

        flexible.dmin = TimeValue::zero();
        flexible.dmax = TimeValue::infinite();
        flexible.finite = false;

        flexible.event_s = tn_point.event;
        flexible.skip_p = scenario.skip;
        flexible.kill_p = scenario.kill;

        flexible.event_min = "emin_" + flexible_name;
        flexible.event_max = "emax_" + flexible_name;
        flexible.event_i = "event_" + event_name;

        flexible.skip = "skip_" + flexible_name;
        flexible.kill = "kill_" + flexible_name;

        scenario.flexibles.push_back(flexible);
        scenario.broadcasts.insert(flexible.event_min);
        scenario.broadcasts.insert(flexible.event_max);
        scenario.broadcasts.insert(flexible.skip);
        scenario.broadcasts.insert(flexible.kill);
    }
}

void TAVisitor::visit(const EventModel &event)
{
    qDebug() << space() << "Visiting event" << event.id();
    event.states();

    const auto& timenode = parentTimeNode(event, scenario.iscore_scenario);
    QString tn_name = name(timenode);
    QString event_name = name(event);

    QString flexible_name = tn_name + "__to__" + event_name;

    TA::Point point{event_name};

    // TODO condition
    // TODO states

    point.en = "en_" + event_name;
    point.event = "event_" + event_name;
    point.skip = "skip_" + event_name;
    point.event_t = "ok_" + event_name;

    point.condition = 0;
    point.conditionMessage = "msg_true";

    point.event_s = "emin_" + flexible_name;
    point.event_e = "emax_" + flexible_name;
    point.skip_p = "skip_" + flexible_name;
    point.kill_p = scenario.kill;

    point.urgent = false;
    scenario.points.push_back(point);

    scenario.broadcasts.insert(point.en);
    scenario.broadcasts.insert(point.event);
    scenario.broadcasts.insert(point.skip);
    scenario.broadcasts.insert(point.event_t);

    // We already linked the start of this event, with
    // the end of the flexible created in the timenode pass
}

void TAVisitor::visit(const StateModel &state)
{
    qDebug() << space() << "Visiting state" << state.id();
    /*
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
    */
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
    QString end_node_name = name(endTimeNode(c, scenario.iscore_scenario));

    QString cst_name = name(c);

    if(c.duration.isRigid())
    {
        // Setup of the rigid
        TA::Rigid rigid{cst_name};
        rigid.dur = c.duration.defaultDuration();

        rigid.event_s = "event_" + start_event_name;
        rigid.event_e1 = "event_" + end_node_name;
        rigid.event_e2 = "event_" + end_node_name;

        rigid.skip_p = scenario.skip;
        rigid.kill_p = scenario.kill;

        rigid.skip = "skip_" + cst_name;
        rigid.kill = "kill_" + cst_name;

        // Register all the new elements
        scenario.rigids.push_back(rigid);
        scenario.broadcasts.insert(rigid.event_e1);
        scenario.broadcasts.insert(rigid.event_e2);
        scenario.broadcasts.insert(rigid.skip);
        scenario.broadcasts.insert(rigid.kill);

        // Link with the end points
        auto it = find_if(scenario.points,
                          [&] (const auto& pt) { return pt.event == rigid.event_e1; });
        if(it != scenario.points.end())
        {
            TA::Point& pt = *it;
            pt.event_s = rigid.event_e1;
            pt.skip_p = rigid.skip;
            pt.event_e = rigid.event_e2;
        }

        visitProcesses(c, rigid, scenario);
    }
    else
    {
        TA::Flexible flexible{cst_name};

        flexible.dmin = c.duration.minDuration();
        flexible.dmax = c.duration.maxDuration();
        flexible.finite = !c.duration.isMaxInfinite();

        flexible.event_s = "event_" + start_event_name;
        flexible.skip_p = scenario.skip;
        flexible.kill_p = scenario.kill;

        QString cst_name = name(c);
        flexible.event_min = "min_" + cst_name;
        flexible.event_max = "max_" + cst_name;
        flexible.event_i = "event_" + end_node_name;

        flexible.skip = "skip_" + cst_name;
        flexible.kill = "kill_" + cst_name;

        scenario.flexibles.push_back(flexible);
        scenario.broadcasts.insert(flexible.event_min);
        scenario.broadcasts.insert(flexible.event_max);
        scenario.broadcasts.insert(flexible.skip);
        scenario.broadcasts.insert(flexible.kill);

        auto it = find_if(scenario.points, [&] (const auto& pt) { return pt.event == flexible.event_i; });
        if(it != scenario.points.end())
        {
            TA::Point& pt = *it;
            pt.event_s = flexible.event_min;
            pt.skip_p = flexible.skip;
            pt.event_e = flexible.event_max;
        }

        visitProcesses(c, flexible, scenario);
    }

    depth ++;

    depth --;
}
