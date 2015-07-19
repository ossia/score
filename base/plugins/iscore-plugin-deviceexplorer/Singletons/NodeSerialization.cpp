#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const Node& n)
{
    m_stream << (int)n.type();
    switch(n.type())
    {
        case Node::Type::Address:
            m_stream << n.addressSettings();
            break;
        case Node::Type::Device:
            m_stream << n.deviceSettings();
            break;
        default:
            break;
    }

    m_stream << n.childCount();
    for(auto& child : n.children())
    {
        if(child) readFrom(*child);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Node& n)
{
    int type;
    int childCount;

    m_stream >> type;
    switch(Node::Type(type))
    {
        case Node::Type::Address:
        {
            AddressSettings s;
            m_stream >> s;
            n.setAddressSettings(s);
            break;
        }
        case Node::Type::Device:
        {
            DeviceSettings s;
            m_stream >> s;
            n.setDeviceSettings(s);
            break;
        }
        default:
            break;
    }

    m_stream >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
        Node* child = new Node;
        writeTo(*child);
        n.addChild(child);
    }

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Node& n)
{
    switch(n.type())
    {
        case Node::Type::Address:
            m_obj["AddressSettings"] = toJsonObject(n.addressSettings());
            break;
        case Node::Type::Device:
            m_obj["DeviceSettings"] = toJsonObject(n.deviceSettings());
            break;
        default:
            break;
    }

    m_obj["Children"] = toJsonArray(n.children());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Node& n)
{
    if(m_obj.contains("AddressSettings"))
    {
        AddressSettings s;
        fromJsonObject(m_obj["AddressSettings"].toObject(), s);
        n.setAddressSettings(s);
    }
    else if(m_obj.contains("DeviceSettings"))
    {
        DeviceSettings s;
        fromJsonObject(m_obj["DeviceSettings"].toObject(), s);
        n.setDeviceSettings(s);
    }

    for (const auto& val : m_obj["Children"].toArray())
    {
        Node* child = new Node;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.addChild(child);
    }
}
