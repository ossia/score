#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>
#include <Document/State/DisplayedStateModel.hpp>
#include <Process/ScenarioModel.hpp>
#include <Document/BaseElement/BaseScenario.hpp>
//using namespace iscore;

InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject* sourceElement,
        QWidget* parentWidget)
{
    if(auto event = dynamic_cast<const EventModel*>(sourceElement))
    {
        return new EventInspectorWidget{event, parentWidget};
    }
    else if(auto state = dynamic_cast<const StateModel*>(sourceElement))
    {
        auto parentElement = state->parent();
        if(auto parentScenar = dynamic_cast<ScenarioModel*>(parentElement))
        {
            auto widg = new EventInspectorWidget{&parentScenar->event(state->eventId()), parentWidget};
            widg->focusState(state);
            return widg;
        }
        else if(auto parentScenar = dynamic_cast<BaseScenario*>(parentElement))
        {
            ISCORE_TODO
        }
    }

    return nullptr;
}
