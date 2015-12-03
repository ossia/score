#pragma once

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>

class ScenarioContextMenuPluginList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ScenarioActionsFactory)
};


