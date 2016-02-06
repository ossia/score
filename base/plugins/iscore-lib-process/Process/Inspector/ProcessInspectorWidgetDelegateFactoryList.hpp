#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <iscore/tools/std/Algorithms.hpp>
class ProcessInspectorWidgetDelegateFactoryList final :
        public iscore::MatchingFactory<ProcessInspectorWidgetDelegateFactory>
{
};

