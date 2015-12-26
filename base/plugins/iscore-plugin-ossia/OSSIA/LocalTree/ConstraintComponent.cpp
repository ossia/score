#include "ConstraintComponent.hpp"

namespace OSSIA
{
namespace LocalTree
{

void make_metadata_node(
        ModelMetadata& metadata,
        OSSIA::Node& parent,
        std::vector<QObject*>& properties)
{

    properties.push_back(
    add_getProperty<QString>(parent, "name", &metadata,
                             &ModelMetadata::name,
                             &ModelMetadata::nameChanged));

    properties.push_back(
    add_property<QString>(parent, "comment", &metadata,
                          &ModelMetadata::comment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::commentChanged));

    properties.push_back(
    add_property<QString>(parent, "label", &metadata,
                          &ModelMetadata::label,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::labelChanged));
}

const iscore::Component::Key&ConstraintComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::ConstraintComponent"};
    return k;
}


ConstraintComponent::ConstraintComponent(
        Node& parent,
        const Id<iscore::Component>& id,
        ConstraintModel& constraint,
        const ConstraintComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "ConstraintComponent", parent_comp},
    m_thisNode{add_node(parent, constraint.metadata.name().toStdString())},
    m_processesNode{add_node(*m_thisNode, "processes")},
    m_baseComponent{*this, constraint, doc, ctx, this}
{
    using tv_t = ::TimeValue;
    make_metadata_node(constraint.metadata, *m_thisNode, m_properties);

    m_properties.push_back(
                add_property<float>(*m_thisNode, "yPos", &constraint,
                        &ConstraintModel::heightPercentage,
                        &ConstraintModel::setHeightPercentage,
                        &ConstraintModel::heightPercentageChanged));

    m_properties.push_back(
    add_getProperty<tv_t>(*m_thisNode, "min", &constraint.duration,
                          &ConstraintDurations::minDuration,
                          &ConstraintDurations::minDurationChanged
                          ));

    m_properties.push_back(
    add_getProperty<tv_t>(*m_thisNode, "max", &constraint.duration,
                          &ConstraintDurations::maxDuration,
                          &ConstraintDurations::maxDurationChanged
                          ));

    m_properties.push_back(
    add_getProperty<tv_t>(*m_thisNode, "default", &constraint.duration,
                          &ConstraintDurations::defaultDuration,
                          &ConstraintDurations::defaultDurationChanged
                          ));

    m_properties.push_back(
    add_getProperty<float>(*m_thisNode, "play", &constraint.duration,
                           &ConstraintDurations::playPercentage,
                           &ConstraintDurations::playPercentageChanged
                           ));
}

ConstraintComponent::~ConstraintComponent()
{
    for(auto prop : m_properties)
        delete prop;
}


ProcessComponent*ConstraintComponent::make_processComponent(const Id<iscore::Component>& id, ProcessComponentFactory& factory, Process& process, const DocumentPlugin& system, const iscore::DocumentContext& ctx, QObject* parent_component)
{
    return factory.make(id, *m_processesNode, process, system, ctx, parent_component);
}


void ConstraintComponent::removing(const Process& cst, const ProcessComponent& comp)
{
    auto it = find_if(m_processesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_processesNode->children().end());

    m_processesNode->children().erase(it);
}

EventComponent::EventComponent(Node& parent, const Id<iscore::Component>& id, EventModel& event, const EventComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent_comp):
    Component{id, "EventComponent", parent_comp},
    m_thisNode{add_node(parent, event.metadata.name().toStdString())}
{
    make_metadata_node(event.metadata, *m_thisNode, m_properties);
}

const iscore::Component::Key&EventComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::EventComponent"};
    return k;
}

EventComponent::~EventComponent()
{
    for(auto prop : m_properties)
        delete prop;
}

TimeNodeComponent::TimeNodeComponent(Node& parent, const Id<iscore::Component>& id, TimeNodeModel& timeNode, const TimeNodeComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent_comp):
    Component{id, "TimeNodeComponent", parent_comp},
    m_thisNode{add_node(parent, timeNode.metadata.name().toStdString())}
{
    make_metadata_node(timeNode.metadata, *m_thisNode, m_properties);


    m_properties.push_back(
    add_setProperty<iscore::impulse_t>(*m_thisNode, "trigger", timeNode.trigger(),
                                       [&] (auto) {
        timeNode.trigger()->triggered();
    }));
}

const iscore::Component::Key&TimeNodeComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::TimeNodeComponent"};
    return k;
}

TimeNodeComponent::~TimeNodeComponent()
{
    for(auto prop : m_properties)
        delete prop;
}

StateComponent::StateComponent(Node& parent, const Id<iscore::Component>& id, StateModel& state, const StateComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent_comp):
    Component{id, "StateComponent", parent_comp},
    m_thisNode{add_node(parent, state.metadata.name().toStdString())}
{
    make_metadata_node(state.metadata, *m_thisNode, m_properties);
}

const iscore::Component::Key&StateComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::StateComponent"};
    return k;
}

StateComponent::~StateComponent()
{
    for(auto prop : m_properties)
        delete prop;
}
}
}
