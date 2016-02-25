#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <iscore/tools/std/Algorithms.hpp>

namespace Process
{
class InspectorWidgetDelegateFactoryList final :
        public iscore::MatchingFactory<Process::InspectorWidgetDelegateFactory>
{
};

class StateProcessInspectorWidgetDelegateFactoryList final :
        public iscore::MatchingFactory<Process::StateProcessInspectorWidgetDelegateFactory>
{
};

}
