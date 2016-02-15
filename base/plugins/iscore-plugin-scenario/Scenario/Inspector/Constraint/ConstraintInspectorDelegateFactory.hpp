#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <memory>
#include <vector>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ConstraintModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorDelegateFactory :
        public iscore::AbstractFactory<ConstraintInspectorDelegateFactory>
{
         ISCORE_ABSTRACT_FACTORY_DECL(
                 ConstraintInspectorDelegateFactory,
                 "e9ae0303-b616-4953-b148-88d2dda5ac45")
    public:
        virtual ~ConstraintInspectorDelegateFactory();
        virtual std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) = 0;
        virtual bool matches(const ConstraintModel& constraint) const = 0;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorDelegateFactoryList final :
        public iscore::MatchingFactory<ConstraintInspectorDelegateFactory>
{
};
}
