#pragma once
#include <src/LocalTree/ComputationComponentFactory.hpp>

namespace Space
{
namespace LocalTree
{

// It must be last in the vector
class GenericComputationComponentFactory final
        : public ComputationComponentFactory
{
    private:
        ComputationComponent* make(
                        const Id<iscore::Component>& cmp,
                        OSSIA::Node& parent,
                        ComputationModel& proc,
                        const Ossia::LocalTree::DocumentPlugin& doc,
                        const iscore::DocumentContext& ctx,
                        QObject* paren_objt) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                ComputationModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;
};

}
}
