#pragma once
#include <src/SpaceProcess.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/std/Algorithms.hpp>

#include <iscore_plugin_space_export.h>
namespace Space
{
namespace LocalTree
{
template<
        typename Component_T, // parent component, of the space process
        typename AreaComponent_T,
        typename ComputationComponent_T,
        typename AreaComponentFactoryList_T,
        typename ComputationsComponentFactoryList_T,
        typename System_T
        >
class SpaceProcessComponentHierarchyManager : public Nano::Observer
{
        using this_t = SpaceProcessComponentHierarchyManager;
        template<typename T, bool dummy = true>
        struct MatchingComponent;

    public:
        SpaceProcessComponentHierarchyManager(
                Component_T& component,
                Space::ProcessModel& space,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* component_as_parent):
            m_process{space},
            m_component{component},
            m_areaComponentFactory{&ctx.app.components.factory<AreaComponentFactoryList_T>()},
            m_computationsComponentFactory{&ctx.app.components.factory<ComputationsComponentFactoryList_T>()},
            m_system{doc},
            m_context{ctx},
            m_parentObject{component_as_parent}
        {
            setup<AreaModel>();
            setup<ComputationModel>();
        }

        ~SpaceProcessComponentHierarchyManager()
        {
            for(auto element : m_computations)
                cleanup(element);
            for(auto element : m_areas)
                cleanup(element);
        }

    private:
        struct AreaPair {
                using element_t = AreaModel;
                AreaModel& element;
                AreaComponent_T& component;
        };
        struct ComputationPair {
                using element_t = ComputationModel;
                ComputationModel& element;
                ComputationComponent_T& component;
        };

        // TODO maybe we can abstract this between Scenario, Space, and Constraint ?
        // We should have some kind of generic handler for a single pair
        template<typename Pair_T>
        void cleanup(const Pair_T& pair)
        {
            m_component.removing(pair.element, pair.component);
            pair.element.components.remove(pair.component);
        }

        template<typename elt_t>
        void setup()
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto& member = m_process.*map_t::process_container;

            for(auto& elt : member)
            {
                add(elt);
            }

            member.mutable_added.template connect<this_t, &this_t::add>(this);
            member.removing.template connect<this_t, &this_t::remove>(this);
        }

        template<typename elt_t>
        void add(elt_t& element)
        {
            using map_t = MatchingComponent<elt_t, true>;
            if(auto factory = (this->*map_t::factory_container)->factory(element, m_system, m_context))
            {
                auto comp = m_component.template make<typename map_t::type>(
                            getStrongId(element.components),
                            *factory,
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
                if(comp)
                {
                    element.components.add(comp);
                    (this->*map_t::local_container).emplace_back(typename map_t::pair_type{element, *comp});
                }
            }
        }

        template<typename elt_t>
        void remove(const elt_t& element)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto& container = this->*map_t::local_container;

            auto it = find_if(container, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != container.end())
            {
                cleanup(*it);
                container.erase(it);
            }
        }

        std::list<AreaPair> m_areas;
        std::list<ComputationPair> m_computations;


        Space::ProcessModel& m_process;
        Component_T& m_component;

        // These are pointers because it is not possible to make ptr-to-member reference.
        const AreaComponentFactoryList_T* m_areaComponentFactory{};
        const ComputationsComponentFactoryList_T* m_computationsComponentFactory{};
        const System_T& m_system;
        const iscore::DocumentContext& m_context;

        QObject* m_parentObject{};

        template<bool dummy>
        struct MatchingComponent<AreaModel, dummy> {
                using type = AreaComponent_T;
                using pair_type = AreaPair;
                static const constexpr auto local_container = &this_t::m_areas;
                static const constexpr auto factory_container = &this_t::m_areaComponentFactory;
                static const constexpr auto process_container = &Space::ProcessModel::areas;
        };
        template<bool dummy>
        struct MatchingComponent<ComputationModel, dummy> {
                using type = ComputationComponent_T;
                using pair_type = ComputationPair;
                static const constexpr auto local_container = &this_t::m_computations;
                static const constexpr auto factory_container = &this_t::m_computationsComponentFactory;
                static const constexpr auto process_container = &Space::ProcessModel::computations;
        };
};


class ISCORE_PLUGIN_SPACE_EXPORT AreaComponent : public iscore::Component
{
    ISCORE_METADATA(Space::LocalTree::AreaComponent)
    public:
        AreaComponent(
                OSSIA::Node& node,
                AreaModel& area,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~AreaComponent();

        const std::shared_ptr<OSSIA::Node>& node() const;

    protected:
        OSSIA::Node& thisNode() const;
    private:
        MetadataNamePropertyWrapper m_thisNode;

    protected:
        std::map<std::string, std::unique_ptr<BaseCallbackWrapper>> m_ginacProperties;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

        AreaModel& m_area;

};
}
}
