#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>

namespace Ossia
{
namespace LocalTree
{

class ScenarioComponentFactory final :
        public ProcessComponentFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("ffc0aaed-9197-4956-a966-3f54afdfb762")
        bool matches(
                Process::ProcessModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};
}
}
