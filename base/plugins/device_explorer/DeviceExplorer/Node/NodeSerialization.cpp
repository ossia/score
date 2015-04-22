#include "Node.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol
             << n.deviceSpecificSettings;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    checkDelimiter();
}
template<>
void Visitor<Reader<JSON>>::readFrom(const DeviceSettings& n)
{

}
template<>
void Visitor<Writer<JSON>>::writeTo(DeviceSettings& n)
{

}


template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressSettings& n)
{

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressSettings& n)
{

    checkDelimiter();
}
template<>
void Visitor<Reader<JSON>>::readFrom(const AddressSettings& n)
{

}
template<>
void Visitor<Writer<JSON>>::writeTo(AddressSettings& n)
{
}



template<>
void Visitor<Reader<DataStream>>::readFrom(const Node& n)
{
    m_stream << n.name()
             << n.value()
             << static_cast<int>(n.ioType())
             << n.minValue()
             << n.maxValue()
             << n.priority();

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
    QString name, value;
    int io;
    float min, max;
    unsigned int prio;
    bool isDev;
    DeviceSettings settings;
    int childCount;

    m_stream >> name >> value >> io >> min >> max >> prio >> isDev;
    if (isDev)
    {
        m_stream >> settings;
    }

    m_stream >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
        Node* child = new Node;
        writeTo(*child);
        n.addChild(child);
    }

    n.setName(name);
    n.setValue(value);
    n.setIOType(static_cast<Node::IOType>(io));
    n.setMinValue(min);
    n.setMaxValue(max);
    n.setPriority(prio);
    n.setDeviceSettings(settings);

    checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const Node& n)
{
    m_obj["Name"] = n.name();
    m_obj["Value"] = n.value();
    m_obj["IOType"] = n.ioType();
    m_obj["MinValue"] = n.minValue();
    m_obj["MaxValue"] = n.maxValue();
    m_obj["Priority"] = static_cast<int>(n.priority());

    if(n.isDevice())
    {
        m_obj["DeviceSettings"] = toJsonObject(n.deviceSettings());
    }

    m_obj["Children"] = toJsonArray(n.children());
}

template<>
void Visitor<Writer<JSON>>::writeTo(Node& timenode)
{
}
