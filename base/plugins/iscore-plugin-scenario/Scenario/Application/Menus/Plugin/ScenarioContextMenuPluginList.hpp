#pragma once

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>

namespace Scenario
{
class ScenarioContextMenuPluginList final :
        public iscore::ConcreteFactoryList<ScenarioActionsFactory>
{
};
}
