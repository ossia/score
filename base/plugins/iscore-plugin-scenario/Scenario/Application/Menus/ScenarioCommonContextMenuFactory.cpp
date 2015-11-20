#include "ScenarioCommonContextMenuFactory.hpp"

#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ToolMenuActions.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>


QList<ScenarioActions *> ScenarioCommonActionsFactory::make(ScenarioApplicationPlugin *ctrl)
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
