#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const Node& n)
{
    m_stream << n.addressSettings();

    m_stream << n.isDevice();
    if(n.isDevice())
    {
        m_stream << n.deviceSettings();
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
    bool isDev;
    int childCount;

    m_stream >> n.m_addressSettings
             >> isDev;
    if (isDev)
    {
        m_stream >> n.m_deviceSettings;
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
    m_obj["AddressSettings"] = toJsonObject(n.addressSettings());

    if(n.isDevice())
    {
        m_obj["DeviceSettings"] = toJsonObject(n.deviceSettings());
    }

    m_obj["Children"] = toJsonArray(n.children());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Node& n)
{
    fromJsonObject(m_obj["AddressSettings"].toObject(), n.m_addressSettings);
    if(m_obj.contains("DeviceSettings"))
    {
        n.setDeviceSettings(fromJsonObject<DeviceSettings>(m_obj["DeviceSettings"].toObject()));
    }

    for (const auto& val : m_obj["Children"].toArray())
    {
        Node* child = new Node;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.addChild(child);
    }
}
