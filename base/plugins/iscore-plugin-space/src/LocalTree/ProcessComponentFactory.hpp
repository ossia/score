#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>

namespace Space
{
namespace LocalTree
{

class ProcessLocalTreeFactory final :
        public Ossia::LocalTree::ProcessComponentFactory
{
    public:
        virtual ~ProcessLocalTreeFactory();
        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        Ossia::LocalTree::ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};

}
}
