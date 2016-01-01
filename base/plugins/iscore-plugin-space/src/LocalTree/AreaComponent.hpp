#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <src/SpaceProcess.hpp>
// TODO Cleanme
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
        > // TODO factory for computations.
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
            m_areaComponentFactory{ctx.app.components.factory<AreaComponentFactoryList_T>()},
            m_computationsComponentFactory{ctx.app.components.factory<ComputationsComponentFactoryList_T>()},
            m_system{doc},
            m_context{ctx},
            m_parentObject{component_as_parent}
        {
            setup<AreaModel>();
            setup<ComputationModel>();
        }

        ~SpaceProcessComponentHierarchyManager()
        {
            for(auto element : m_areas)
                cleanup(element);
            for(auto element : m_computations)
                cleanup(element);
        }

    private:
        struct AreaPair {
                using element_t = ConstraintModel;
                AreaModel& element;
                AreaComponent_T& component;
        };
        struct ComputationPair {
                using element_t = EventModel;
                ComputationModel& element;
                ComputationComponent_T& component;
        };

        // TODO maybe we can abstract this between Scenario, Space, and Constraint ?
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
            auto comp = m_component.template make<elt_t, typename map_t::type>(
                            getStrongId(element.components),
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

        template<bool dummy>
        struct MatchingComponent<AreaModel, dummy> {
                using type = AreaComponent_T;
                using pair_type = AreaPair;
                static const constexpr auto local_container = &this_t::m_areas;
                static const constexpr auto process_container = &Space::ProcessModel::areas;
        };
        template<bool dummy>
        struct MatchingComponent<EventModel, dummy> {
                using type = ComputationComponent_T;
                using pair_type = ComputationPair;
                static const constexpr auto local_container = &this_t::m_computations;
                static const constexpr auto process_container = &Space::ProcessModel::computations;
        };

        Space::ProcessModel& m_process;
        ConstraintModel& m_constraint;
        Component_T& m_component;
        const AreaComponentFactoryList_T& m_areaComponentFactory;
        const ComputationsComponentFactoryList_T& m_computationsComponentFactory;
        const System_T& m_system;
        const iscore::DocumentContext& m_context;

        QObject* m_parentObject{};
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
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

    private:
        OSSIA::Node& thisNode() const;
        MetadataNamePropertyWrapper m_thisNode;
};

class ISCORE_PLUGIN_SPACE_EXPORT AreaComponentFactory :
        public ::GenericComponentFactory<
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
    ::GenericComponentFactoryList<
            AreaModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::AreaComponent,
            Space::LocalTree::AreaComponentFactory>;


class ISCORE_PLUGIN_SPACE_EXPORT ComputationComponent : public iscore::Component
{
    ISCORE_METADATA(Space::LocalTree::ComputationComponent)
    public:
        ComputationComponent(
                OSSIA::Node& node,
                ComputationModel& Computation,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~ComputationComponent();

        const std::shared_ptr<OSSIA::Node>& node() const;

    protected:
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

    private:
        OSSIA::Node& thisNode() const;
        MetadataNamePropertyWrapper m_thisNode;
};

class ISCORE_PLUGIN_SPACE_EXPORT ComputationComponentFactory :
        public ::GenericComponentFactory<
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
    ::GenericComponentFactoryList<
            ComputationModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::ComputationComponent,
            Space::LocalTree::ComputationComponentFactory>;

using ComputationComponentFactoryList =
    ::GenericComponentFactoryList<
            ComputationModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::ComputationComponent,
            Space::LocalTree::ComputationComponentFactory>;

class GenericAreaComponent final : public AreaComponent
{
        COMPONENT_METADATA(GenericAreaComponent)
    public:
        GenericAreaComponent(
                const Id<iscore::Component>& cmp,
                OSSIA::Node& parent_node,
                AreaModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt);
};

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

        std::shared_ptr<OSSIA::Node> m_areas;
        std::shared_ptr<OSSIA::Node> m_computations;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

};


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
