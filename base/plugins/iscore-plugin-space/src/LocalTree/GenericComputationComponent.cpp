#include "GenericComputationComponent.hpp"
#include <OSSIA/LocalTree/Scenario/MetadataParameters.cpp>

namespace Space
{
namespace LocalTree
{

GenericComputationComponent::GenericComputationComponent(
        const Id<iscore::Component>& cmp,
        OSSIA::Node& parent_node,
        ComputationModel& computation,
        const Ossia::LocalTree::DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt):
    ComputationComponent{parent_node, computation, cmp, "GenericComputationComponent", paren_objt}
{
    Ossia::LocalTree::make_metadata_node(computation.metadata, *node(), m_properties, this);

    using namespace GiNaC;
    constexpr auto t = Ossia::convert::MatchingType<double>::val;
    auto node_it = thisNode().emplaceAndNotify(
                       thisNode().children().end(),
                       computation.metadata.name().toStdString(),
                       t,
                       OSSIA::AccessMode::GET);

    m_valueNode = *node_it;
    auto addr = m_valueNode->getAddress();
    addr->setValue(iscore::convert::toOSSIAValue(
                       State::Value::fromValue(computation.computation()())));

}

}
}
