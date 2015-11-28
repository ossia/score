#include "ScenarioVisitor.hpp"
#include <iscore/menu/MenuInterface.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>

#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <core/application/Application.hpp>

struct TAVisitor
{
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

        void visit(const EventModel& event)
        {
            qDebug() << space() << "Visiting event" << event.id();
            event.states();
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
            depth++;
            for(const TimeNodeModel& timenode : s.timeNodes)
            {
                visit(timenode);
            }

            for(const EventModel& event : s.events)
            {
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

        void visit(const ConstraintModel& c)
        {
            qDebug() << space() << "Visiting constraint" << c.id();
            if(c.duration.isRigid())
            {
                c.duration.defaultDuration();
            }
            else
            {
                c.duration.minDuration();
                c.duration.maxDuration();
            }

            depth ++;
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

            depth --;
        }


};

TemporalAutomatas::ApplicationPlugin::ApplicationPlugin(iscore::Application& app):
    iscore::GUIApplicationContextPlugin(app, "TemporalAutomatasApplicationPlugin", &app)
{
    m_convert = new QAction{tr("Convert to Temporal Automatas"), nullptr};
    connect(m_convert, &QAction::triggered, [&] () {
        auto doc = currentDocument();
        if(!doc)
            return;
        ScenarioDocumentModel& base = iscore::IDocument::get<ScenarioDocumentModel>(*doc);

        TAVisitor v;

        v.visit(base.baseScenario().constraint());
    } );
}


void TemporalAutomatas::ApplicationPlugin::populateMenus(iscore::MenubarManager* menus)
{
    menus->insertActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                m_convert);
}
