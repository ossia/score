#include <DeviceExplorer/Node/Node.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp"



template<>
void Visitor<Reader<DataStream>>::readFrom(const DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());

    checkDelimiter();
}
template<>
void Visitor<Reader<JSON>>::readFrom(const DeviceSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["Protocol"] = n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
}

template<>
void Visitor<Writer<JSON>>::writeTo(DeviceSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.protocol = m_obj["Protocol"].toString();

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressSettings& n)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressSettings& n)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    checkDelimiter();
}
template<>
void Visitor<Reader<JSON>>::readFrom(const AddressSettings& n)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
template<>
void Visitor<Writer<JSON>>::writeTo(AddressSettings& n)
{
    qDebug() << Q_FUNC_INFO << "TODO";
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

    if(isDev)
        n.setDeviceSettings(settings);

    n.setName(name);
    n.setValue(value);
    n.setIOType(static_cast<Node::IOType>(io));
    n.setMinValue(min);
    n.setMaxValue(max);
    n.setPriority(prio);

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
void Visitor<Writer<JSON>>::writeTo(Node& n)
{
    if(m_obj.contains("DeviceSettings"))
    {
        n.setDeviceSettings(fromJsonObject<DeviceSettings>(m_obj["DeviceSettings"].toObject()));
    }

    n.setName(m_obj["Name"].toString());
    n.setValue(m_obj["Value"].toString());
    n.setIOType(static_cast<Node::IOType>(m_obj["IOType"].toInt()));
    n.setMinValue(m_obj["MinValue"].toInt());
    n.setMaxValue(m_obj["MaxValue"].toInt());
    n.setPriority(m_obj["Priority"].toInt());

    for (const auto& val : m_obj["Children"].toArray())
    {
        Node* child = new Node;
        Deserializer<JSON> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.addChild(child);
    }

}
