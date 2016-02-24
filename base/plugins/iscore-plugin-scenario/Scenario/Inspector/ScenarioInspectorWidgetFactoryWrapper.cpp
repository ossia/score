#include "ScenarioInspectorWidgetFactoryWrapper.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <Scenario/Inspector/TimeNode/TimeNodeInspectorWidget.hpp>
#include <Scenario/Inspector/Summary/SummaryInspectorWidget.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorFactory.hpp>

namespace Scenario
{
Inspector::InspectorWidgetBase* ScenarioInspectorWidgetFactoryWrapper::makeWidget(
        QList<const QObject*> sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    std::set<const ConstraintModel*> constraints;
    std::set<const TimeNodeModel*> timenodes;
    std::set<const EventModel*> events;
    std::set<const StateModel*> states;

    const IdentifiedObjectAbstract* abstr; // the first matching element (default case purpose)

    ScenarioInterface* scenar{};
    for(auto elt : sourceElements)
    {
        if(dynamic_cast<const StateModel*>(elt) ||
                dynamic_cast<const EventModel*>(elt) ||
                dynamic_cast<const TimeNodeModel*>(elt) ||
                dynamic_cast<const ConstraintModel*>(elt))
        {
            scenar = dynamic_cast<ScenarioInterface*>(elt->parent());
            abstr = dynamic_cast<const IdentifiedObjectAbstract*>(elt);
            break;
        }
    }
    ISCORE_ASSERT(scenar); // because else, matches should have return false
    for(auto elt : sourceElements)
    {
        if(auto st = dynamic_cast<const StateModel*>(elt))
        {
            auto ev = &scenar->event(st->eventId());
            auto tn = &scenar->timeNode(ev->timeNode());
            states.insert(st);
            events.insert(ev);
            timenodes.insert(tn);
        }
        else if (auto ev = dynamic_cast<const EventModel*>(elt))
        {
            auto tn = &scenar->timeNode(ev->timeNode());
            events.insert(ev);
            timenodes.insert(tn);
        }
        else if (auto tn = dynamic_cast<const TimeNodeModel*>(elt))
        {
            timenodes.insert(tn);
        }
        else if (auto cstr = dynamic_cast<const ConstraintModel*>(elt))
        {
            constraints.insert(cstr);
        }
    }

    if(timenodes.size() == 1 && constraints.size() == 0)
        return new TimeNodeInspectorWidget{**timenodes.begin(), doc, parent};

    if(constraints.size() == 1 && timenodes.size() == 0)
    {
        auto f = new ConstraintInspectorFactory{};
        return f->makeWidget({*constraints.begin()}, doc, parent);
    }

    return new SummaryInspectorWidget{
        abstr, constraints, timenodes, events, states, doc, parent
    }; // the default InspectorWidgetBase need an only IdentifiedObject : this will be "abstr"
}

bool ScenarioInspectorWidgetFactoryWrapper::matches(
        QList<const QObject*> objects) const
{
    if(std::any_of(objects.begin(), objects.end(),
                   [] (const QObject* obj) {
                   return dynamic_cast<const StateModel*>(obj) ||
                            dynamic_cast<const EventModel*>(obj) ||
                            dynamic_cast<const TimeNodeModel*>(obj) ||
                            dynamic_cast<const ConstraintModel*>(obj);
                    }))
    {
        return true;
    }

    return false;
}
}
