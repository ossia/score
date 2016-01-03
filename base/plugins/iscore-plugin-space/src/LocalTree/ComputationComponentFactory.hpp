#pragma once
#include <src/LocalTree/ComputationComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <iscore/component/ComponentFactory.hpp>

namespace Space
{
namespace LocalTree
{

class ISCORE_PLUGIN_SPACE_EXPORT ComputationComponentFactory :
        public iscore::GenericComponentFactory<
            ComputationModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::ComputationComponent>
{
    public:
        virtual ~ComputationComponentFactory();

        virtual ComputationComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                ComputationModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};

// TODO return Generic by default
using ComputationComponentFactoryList =
    iscore::GenericComponentFactoryList<
            ComputationModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::ComputationComponent,
            Space::LocalTree::ComputationComponentFactory>;

}
}
