#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const DeviceExplorerNode& n)
{
    m_stream << (int)n.type();
    switch(n.type())
    {
        case DeviceExplorerNode::Type::Address:
            m_stream << n.get<AddressSettings>();
            break;
        case DeviceExplorerNode::Type::Device:
            m_stream << n.get<DeviceSettings>();
            break;
        default:
            break;
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(DeviceExplorerNode& n)
{
    int type;

    m_stream >> type;
    switch(DeviceExplorerNode::Type(type))
    {
        case DeviceExplorerNode::Type::Address:
        {
            AddressSettings s;
            m_stream >> s;
            n.set(s);
            break;
        }
        case DeviceExplorerNode::Type::Device:
        {
            DeviceSettings s;
            m_stream >> s;
            n.set(s);
            break;
        }
        default:
            break;
    }

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const DeviceExplorerNode& n)
{
    switch(n.type())
    {
        case Node::Type::Address:
            m_obj["AddressSettings"] = toJsonObject(n.get<AddressSettings>());
            break;
        case Node::Type::Device:
            m_obj["DeviceSettings"] = toJsonObject(n.get<DeviceSettings>());
            break;
        default:
            break;
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(DeviceExplorerNode& n)
{
    if(m_obj.contains("AddressSettings"))
    {
        AddressSettings s;
        fromJsonObject(m_obj["AddressSettings"].toObject(), s);
        n.set(s);
    }
    else if(m_obj.contains("DeviceSettings"))
    {
        DeviceSettings s;
        fromJsonObject(m_obj["DeviceSettings"].toObject(), s);
        n.set(s);
    }
}
