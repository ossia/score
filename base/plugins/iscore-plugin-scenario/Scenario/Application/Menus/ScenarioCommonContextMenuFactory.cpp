#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ToolMenuActions.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>

#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>
#include "ScenarioCommonContextMenuFactory.hpp"
#include <iscore/menu/MenuInterface.hpp>


namespace Scenario
{
QList<ScenarioActions *> ScenarioCommonActionsFactory::make(ScenarioApplicationPlugin *ctrl)
{
    return QList<ScenarioActions *>{
        new ObjectMenuActions(iscore::ToplevelMenuElement::ObjectMenu, ctrl),
        new ToolMenuActions(iscore::ToplevelMenuElement::ToolMenu, ctrl),
        new TransportActions(iscore::ToplevelMenuElement::PlayMenu, ctrl)
    };
}

const ScenarioActionsFactoryKey&ScenarioCommonActionsFactory::concreteFactoryKey() const
{
    static const ScenarioActionsFactoryKey fact{"f6ac9feb-4ffd-4fab-93d4-b5834a078c6e"};
    return fact;
}
}
