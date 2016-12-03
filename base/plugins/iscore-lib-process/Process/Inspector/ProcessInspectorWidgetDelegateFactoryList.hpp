#pragma once
#include <ossia/detail/algorithms.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Process
{
class InspectorWidgetDelegateFactoryList final
    : public iscore::MatchingFactory<Process::InspectorWidgetDelegateFactory>
{
};

class StateProcessInspectorWidgetDelegateFactoryList final
    : public iscore::
          MatchingFactory<Process::StateProcessInspectorWidgetDelegateFactory>
{
};
}
