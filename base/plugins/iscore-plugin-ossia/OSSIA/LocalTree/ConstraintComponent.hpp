#pragma once
#include <OSSIA/LocalTree/ProcessComponent.hpp>

namespace OSSIA
{
namespace LocalTree
{

inline void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent)
{
    add_getProperty<QString>(parent, "name", &metadata,
                             &ModelMetadata::name,
                             &ModelMetadata::nameChanged);
    add_property<QString>(parent, "comment", &metadata,
                          &ModelMetadata::comment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::commentChanged);
    add_property<QString>(parent, "label", &metadata,
                          &ModelMetadata::label,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::labelChanged);
}

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

        const Key& key() const override
        {
            static const Key k{"OSSIA::LocalTree::ConstraintComponent"};
            return k;
        }

        ConstraintComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                ConstraintModel& constraint,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp):
            Component{id, "ConstraintComponent", parent_comp},
            m_thisNode{add_node(parent, constraint.metadata.name().toStdString())},
            m_processesNode{add_node(*m_thisNode, "processes")},
            m_baseComponent{*this, constraint, doc, ctx, this}
        {
            using tv_t = ::TimeValue;
            make_metadata_node(constraint.metadata, *m_thisNode);

            //visit(constraint.metadata, m_thisNode);
            add_property<float>(*m_thisNode, "yPos", &constraint,
                                &ConstraintModel::heightPercentage,
                                &ConstraintModel::setHeightPercentage,
                                &ConstraintModel::heightPercentageChanged);

            add_getProperty<tv_t>(*m_thisNode, "min", &constraint.duration,
                                  &ConstraintDurations::minDuration,
                                  &ConstraintDurations::minDurationChanged
                                  );
            add_getProperty<tv_t>(*m_thisNode, "max", &constraint.duration,
                                  &ConstraintDurations::maxDuration,
                                  &ConstraintDurations::maxDurationChanged
                                  );
            add_getProperty<tv_t>(*m_thisNode, "default", &constraint.duration,
                                  &ConstraintDurations::defaultDuration,
                                  &ConstraintDurations::defaultDurationChanged
                                  );
            add_getProperty<float>(*m_thisNode, "play", &constraint.duration,
                                   &ConstraintDurations::playPercentage,
                                   &ConstraintDurations::playPercentageChanged
                                   );
        }

        ProcessComponent* make_processComponent(
                const Id<Component> & id,
                ProcessComponentFactory& factory,
                Process &process,
                const DocumentPlugin &system,
                const iscore::DocumentContext &ctx,
                QObject *parent_component)
        {
            //auto it = add_node(*m_processesNode, process.metadata.name().toStdString());
            return factory.make(id, *m_processesNode, process, system, ctx, parent_component);
        }

        void removing(const Process& cst, const ProcessComponent& comp)
        {
            auto it = find_if(m_processesNode->children(), [&] (const auto& node)
            { return node == comp.node(); });
            ISCORE_ASSERT(it != m_processesNode->children().end());

            m_processesNode->children().erase(it);
        }

        auto& node() const
        { return m_thisNode; }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::shared_ptr<OSSIA::Node> m_processesNode;
        parent_t m_baseComponent;
};




class EventComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        EventComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                EventModel& event,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp):
            Component{id, "EventComponent", parent_comp},
            m_thisNode{add_node(parent, event.metadata.name().toStdString())}
        {
            make_metadata_node(event.metadata, *m_thisNode);
        }

        const Key& key() const override
        {
            static const Key k{"OSSIA::LocalTree::EventComponent"};
            return k;
        }

        auto& node() const
        { return m_thisNode; }
};

class TimeNodeComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        TimeNodeComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                TimeNodeModel& timeNode,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp):
            Component{id, "TimeNodeComponent", parent_comp},
            m_thisNode{add_node(parent, timeNode.metadata.name().toStdString())}
        {
            make_metadata_node(timeNode.metadata, *m_thisNode);

            add_setProperty<iscore::impulse_t>(*m_thisNode, "trigger", timeNode.trigger(),
                                       [&] (auto) {
                timeNode.trigger()->triggered();
            });
        }

        const Key& key() const override
        {
            static const Key k{"OSSIA::LocalTree::TimeNodeComponent"};
            return k;
        }

        auto& node() const
        { return m_thisNode; }
};

class StateComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        StateComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                StateModel& state,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp):
            Component{id, "StateComponent", parent_comp},
            m_thisNode{add_node(parent, state.metadata.name().toStdString())}
        {
            make_metadata_node(state.metadata, *m_thisNode);
        }

        const Key& key() const override
        {
            static const Key k{"OSSIA::LocalTree::StateComponent"};
            return k;
        }

        auto& node() const
        { return m_thisNode; }
};

}
}
