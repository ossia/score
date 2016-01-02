#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>

namespace Ossia
{
namespace LocalTree
{

class ScenarioComponentFactory final :
        public ProcessComponentFactory
{
    public:
        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                const DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};
}
}
