#pragma once
#include <src/LocalTree/AreaComponent.hpp>
#include <iscore/component/ComponentFactory.hpp>

namespace Space
{
namespace LocalTree
{
class ISCORE_PLUGIN_SPACE_EXPORT AreaComponentFactory :
        public iscore::GenericComponentFactory<
        AreaModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::AreaComponent>
{
    public:
        virtual ~AreaComponentFactory();

        virtual AreaComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                AreaModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};



// TODO return Generic by default
using AreaComponentFactoryList =
    iscore::GenericComponentFactoryList<
            AreaModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::AreaComponent,
            Space::LocalTree::AreaComponentFactory>;

}
}
