#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <QChar>
#include <QDebug>

#include <QString>
#include <sstream>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include "ScenarioVisitor.hpp"
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/Algorithms.hpp>

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

namespace TA
{
using Event = QString;
struct Point
{
        const EventModel& iscore_event;
        int condition{};
        int conditionValue{};
        Event en; // Enabled
        Event conditionMessage;

        Event event;

        bool urgent{};

        Event event_s;
        Event skip_p;
        Event event_e;
        Event kill_p;

        Event skip;
        Event event_t;
};

template<typename Stream>
void print(const Point& pt, Stream& stream)
{
    QString s = QString("%1 = Point(%2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13)")
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

struct Flexible
{
        const ConstraintModel& iscore_constraint;

        TimeValue dmin;
        TimeValue dmax;
        bool finite{};

        Event event_s;
        Event event_min;
        Event event_i;
        Event event_max;

        Event skip_p;
        Event kill_p;
        Event skip;
        Event kill;
};

template<typename Stream>
void print(const Flexible& c, Stream& stream)
{
    QString s = QString("%1 = Flexible(%2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12)")
            .arg(name(c.iscore_constraint))
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


struct Rigid
{
        const ConstraintModel& iscore_constraint;

        TimeValue dur;

        Event event_s;
        Event event_e1;

        Event skip_p;
        Event kill_p;
        Event skip;
        Event kill;

        Event event_e2;
};

struct TAScenario
{
        TAScenario(const Scenario::ScenarioModel& s):
            iscore_scenario{s}
        {
            events.push_back(skip_S);
            events.push_back(kill_S);
        }

        const Scenario::ScenarioModel& iscore_scenario;

        const TA::Event skip_S = "skip_S" + name(iscore_scenario);
        const TA::Event kill_S = "kill_S" + name(iscore_scenario);

        std::vector<TA::Event> events;
        std::vector<TA::Flexible> flexibles;
        std::vector<TA::Point> points;
};
}
struct TAVisitor
{
        std::stringstream output;
        int depth = 0;

        auto space() const
        {
            return qPrintable(QString(depth, '-'));
        }

        void visit(const AutomationModel& automation)
        {
            qDebug() << space() << "Visiting automation" << automation.id();
            automation.duration(); // In ms
            automation.address();
            automation.min();
            automation.max();

            for(const auto& segment : Curve::orderedSegments(automation.curve()))
            {
                segment.start; // x, y
                segment.end;
                segment.previous; // id of previous segment
                segment.following; // id of following segment
            }
        }

        void visit(const TimeNodeModel& timenode)
        {
            qDebug() << space() << "Visiting timenode" << timenode.id();
            auto trigger = timenode.trigger();
            trigger->expression();

            timenode.events();
        }

        void visit(
                const EventModel& event,
                TA::TAScenario& scenario)
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

            point.urgent = false;
            scenario.points.push_back(point);

            scenario.events.push_back(point.en);
            scenario.events.push_back(point.event);
            scenario.events.push_back(point.skip);
            scenario.events.push_back(point.event_t);


        }

        void visit(const StateModel& state)
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

        void visit(const Scenario::ScenarioModel& s)
        {
            qDebug() << space() << "Visiting scenario" << s.id();
            TA::TAScenario ta_scenario{s};


            depth++;
            for(const TimeNodeModel& timenode : s.timeNodes)
            {
                visit(timenode);
            }

            for(const EventModel& event : s.events)
            {
                visit(event, ta_scenario);
            }

            for(const StateModel& state : s.states)
            {
                visit(state);
            }

            for(const ConstraintModel& constraint : s.constraints)
            {
                visit(constraint, ta_scenario);
            }
            depth--;

            print(ta_scenario.flexibles, output);
            print(ta_scenario.points, output);
        }

        void visit(const ConstraintModel& c,
                   TA::TAScenario& scenario)
        {
            qDebug() << space() << "Visiting constraint" << c.id();

            QString start_event_name = name(startEvent(c, scenario.iscore_scenario));
            QString end_event_name = name(endEvent(c, scenario.iscore_scenario));

            TA::Flexible flexible{c};

            // TODO
            flexible.dmin = c.duration.defaultDuration();
            flexible.dmax = c.duration.defaultDuration();

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
            scenario.events.push_back(flexible.event_min);
            scenario.events.push_back(flexible.event_max);
            scenario.events.push_back(flexible.skip);
            scenario.events.push_back(flexible.kill);

            auto it = find_if(scenario.points, [&] (const auto& pt) { return pt.event == flexible.event_i; });
            if(it != scenario.points.end())
            {
                TA::Point& pt = *it;
                pt.event_s = flexible.event_min;
                pt.skip_p = flexible.skip;
                pt.event_e = flexible.event_max;
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
};

#include <Scenario/Application/Menus/TextDialog.hpp>
TemporalAutomatas::ApplicationPlugin::ApplicationPlugin(const iscore::ApplicationContext& app):
    iscore::GUIApplicationContextPlugin(app, "TemporalAutomatasApplicationPlugin", nullptr)
{
    m_convert = new QAction{tr("Convert to Temporal Automatas"), nullptr};
    connect(m_convert, &QAction::triggered, [&] () {
        auto doc = currentDocument();
        if(!doc)
            return;
        ScenarioDocumentModel& base = iscore::IDocument::get<ScenarioDocumentModel>(*doc);

        TAVisitor v;

        v.visit(static_cast<Scenario::ScenarioModel&>(*base.baseScenario().constraint().processes.begin()));

        TextDialog dial(QString::fromStdString(v.output.str()), qApp->activeWindow());
        dial.exec();
    } );
}

void TemporalAutomatas::ApplicationPlugin::populateMenus(iscore::MenubarManager* menus)
{
    menus->insertActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                m_convert);
}
