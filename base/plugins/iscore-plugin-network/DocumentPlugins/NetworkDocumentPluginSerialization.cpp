#include "NetworkDocumentPlugin.hpp"
#include "PlaceholderNetworkPolicy.hpp"
#include "DistributedScenario/GroupManager.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const NetworkDocumentPlugin& elt)
{
    readFrom(*elt.groupManager());
    readFrom(*elt.policy());

    // Note : we do not save the policy since it will be different on each computer.
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(NetworkDocumentPlugin& elt)
{
    elt.m_groups = new GroupManager{*this, &elt};
    elt.m_policy = new PlaceholderNetworkPolicy{*this, &elt};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const NetworkDocumentPlugin& elt)
{
    readFrom(*elt.groupManager());
    readFrom(*elt.policy());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(NetworkDocumentPlugin& elt)
{
    elt.m_groups = new GroupManager{*this, &elt};
    elt.m_policy = new PlaceholderNetworkPolicy{*this, &elt};
}
