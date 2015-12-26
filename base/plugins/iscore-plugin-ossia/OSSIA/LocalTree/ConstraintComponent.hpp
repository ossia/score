#pragma once
#include <OSSIA/LocalTree/ProcessComponent.hpp>

namespace OSSIA
{
namespace LocalTree
{

void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent,
        std::vector<QObject*>& properties);

class ConstraintComponent final :
        public iscore::Component
{
    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;
        using process_component_t = OSSIA::LocalTree::ProcessComponent;
        using process_component_factory_t = OSSIA::LocalTree::ProcessComponentFactory;
        using process_component_factory_list_t = OSSIA::LocalTree::ProcessComponentFactoryList;

        using parent_t = ::ConstraintComponentHierarchyManager<
            ConstraintComponent,
            system_t,
            process_component_t,
            process_component_factory_list_t
        >;

        const Key& key() const override;

        ConstraintComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                ConstraintModel& constraint,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);
        ~ConstraintComponent();

        ProcessComponent* make_processComponent(
                const Id<Component> & id,
                ProcessComponentFactory& factory,
                Process &process,
                const DocumentPlugin &system,
                const iscore::DocumentContext &ctx,
                QObject *parent_component);

        void removing(const Process& cst, const ProcessComponent& comp);

        auto& node() const
        { return m_thisNode; }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::shared_ptr<OSSIA::Node> m_processesNode;
        std::vector<QObject*> m_properties;
        parent_t m_baseComponent;
};




class EventComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::vector<QObject*> m_properties;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        EventComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                EventModel& event,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode; }
        ~EventComponent();
};

class TimeNodeComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::vector<QObject*> m_properties;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        TimeNodeComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                TimeNodeModel& timeNode,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode; }
        ~TimeNodeComponent();
};

class StateComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::vector<QObject*> m_properties;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        StateComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                StateModel& state,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode; }
        ~StateComponent();
};

}
}
