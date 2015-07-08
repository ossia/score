#include "ScenarioCommonContextMenuFactory.hpp"

#include "Control/Menus/ObjectMenuActions.hpp"
#include "Control/Menus/ToolMenuActions.hpp"


QList<AbstractMenuActions *> ScenarioCommonContextMenuFactory::make(ScenarioControl *ctrl)
{
    return QList<AbstractMenuActions *>{
        new ObjectMenuActions(iscore::ToplevelMenuElement::ObjectMenu, ctrl),
                new ToolMenuActions(iscore::ToplevelMenuElement::ToolMenu, ctrl)
    };
}
