#pragma once
#include <src/LocalTree/AreaComponentFactory.hpp>
#include <src/LocalTree/ComputationComponentFactory.hpp>

#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>

#include <iscore_plugin_space_export.h>

namespace Space
{
namespace LocalTree
{
class ProcessLocalTree final :
        public Ossia::LocalTree::ProcessComponent
{
        COMPONENT_METADATA(SpaceProcessLocalTree)

         using system_t = Ossia::LocalTree::DocumentPlugin;

        using hierarchy_t =
           SpaceProcessComponentHierarchyManager<
               ProcessLocalTree,
               AreaComponent,
               ComputationComponent,
               AreaComponentFactoryList,
               ComputationComponentFactoryList,
              system_t
        >;

     public:
        ProcessLocalTree(
                const Id<Component>& id,
                OSSIA::Node& parent,
                Space::ProcessModel& process,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_obj);

        template<typename Component_T, typename Element_T, typename Factory_T>
        Component_T* make(
                const Id<Component>& id,
                Factory_T&,
                Element_T& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent);

        void removing(
                const AreaModel& elt,
                const AreaComponent& comp);
        void removing(
                const ComputationModel& elt,
                const ComputationComponent& comp);

        std::shared_ptr<OSSIA::Node> m_areas;
        std::shared_ptr<OSSIA::Node> m_computations;

        std::vector<std::unique_ptr<BaseProperty>> m_properties;

        hierarchy_t m_hm;
};
}
}
