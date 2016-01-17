#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "DistributedScenario/GroupManager.hpp"
#include "NetworkDocumentPlugin.hpp"
#include "PlaceholderNetworkPolicy.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Network::NetworkDocumentPlugin& elt)
{
    readFrom(*elt.groupManager());
    readFrom(*elt.policy());

    // Note : we do not save the policy since it will be different on each computer.
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Network::NetworkDocumentPlugin& elt)
{
    elt.m_groups = new Network::GroupManager{*this, &elt};
    elt.m_policy = new Network::PlaceholderNetworkPolicy{*this, &elt};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Network::NetworkDocumentPlugin& elt)
{
    readFrom(*elt.groupManager());
    readFrom(*elt.policy());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Network::NetworkDocumentPlugin& elt)
{
    elt.m_groups = new Network::GroupManager{*this, &elt};
    elt.m_policy = new Network::PlaceholderNetworkPolicy{*this, &elt};
}
