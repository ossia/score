#include "ScenarioCommonContextMenuFactory.hpp"

#include "Control/Menus/ObjectMenuActions.hpp"
#include "Control/Menus/ToolMenuActions.hpp"
#include "Control/Menus/TransportActions.hpp"


QList<ScenarioActions *> ScenarioCommonActionsFactory::make(ScenarioControl *ctrl)
{
    return QList<ScenarioActions *>{
        new ObjectMenuActions(iscore::ToplevelMenuElement::ObjectMenu, ctrl),
        new ToolMenuActions(iscore::ToplevelMenuElement::ToolMenu, ctrl),
        new TransportActions(iscore::ToplevelMenuElement::PlayMenu, ctrl)
    };
}
