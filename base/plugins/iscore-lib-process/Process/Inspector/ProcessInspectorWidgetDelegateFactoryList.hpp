#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <iscore/tools/std/Algorithms.hpp>
class ProcessInspectorWidgetDelegateFactoryList final :
        public iscore::ConcreteFactoryList<ProcessInspectorWidgetDelegateFactory>
{
    public:
        auto make(
                const Process::ProcessModel& proc,
                const iscore::DocumentContext& ctx,
                QWidget* parent) const
        {
            auto it = find_if(*this, [&] (const auto& elt) { return elt.matches(proc); });
            return (it != end())
                    ? it->make(proc, ctx, parent)
                    : nullptr;
        }
};

