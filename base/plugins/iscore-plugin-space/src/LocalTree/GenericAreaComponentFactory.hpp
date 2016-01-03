#pragma once
//#include <src/LocalTree/GenericAreaComponent.hpp>
#include <src/LocalTree/AreaComponentFactory.hpp>

namespace Space
{
namespace LocalTree
{

// It must be last in the vector
class GenericAreaComponentFactory final
        : public AreaComponentFactory
{
    private:
        AreaComponent* make(
                        const Id<iscore::Component>& cmp,
                        OSSIA::Node& parent,
                        AreaModel& proc,
                        const Ossia::LocalTree::DocumentPlugin& doc,
                        const iscore::DocumentContext& ctx,
                        QObject* paren_objt) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Space::AreaModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;
};
}
}
