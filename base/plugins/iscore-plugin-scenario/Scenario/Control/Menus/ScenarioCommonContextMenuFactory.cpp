#include "ScenarioCommonContextMenuFactory.hpp"

#include <Scenario/Control/Menus/ObjectMenuActions.hpp>
#include <Scenario/Control/Menus/ToolMenuActions.hpp>
#include <Scenario/Control/Menus/TransportActions.hpp>


QList<ScenarioActions *> ScenarioCommonActionsFactory::make(ScenarioControl *ctrl)
{
    return QList<ScenarioActions *>{
        new ObjectMenuActions(iscore::ToplevelMenuElement::ObjectMenu, ctrl),
        new ToolMenuActions(iscore::ToplevelMenuElement::ToolMenu, ctrl),
        new TransportActions(iscore::ToplevelMenuElement::PlayMenu, ctrl)
    };
}

const ScenarioActionsFactoryKey&ScenarioCommonActionsFactory::key_impl() const
{
    static const ScenarioActionsFactoryKey fact{"ScenarioCommonActionsFactory"};
    return fact;
}
