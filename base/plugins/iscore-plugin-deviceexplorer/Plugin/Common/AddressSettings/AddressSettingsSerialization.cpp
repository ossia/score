#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "AddressSettings.hpp"
#include "AddressSpecificSettings/AddressFloatSettings.hpp"
#include "AddressSpecificSettings/AddressIntSettings.hpp"
#include "AddressSpecificSettings/AddressStringSettings.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressFloatSettings& n)
{
    m_stream << n.min << n.max << n.unit << n.clipMode;
    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressFloatSettings& n)
{
    m_stream >> n.min >> n.max >> n.unit >> n.clipMode;
    checkDelimiter();
}
template<>
void Visitor<Reader<JSONObject>>::readFrom(const AddressFloatSettings& n)
{
    m_obj["Min"] = n.min;
    m_obj["Max"] = n.max;
    m_obj["Unit"] = n.unit;
    m_obj["ClipMode"] = n.clipMode;
}
template<>
void Visitor<Writer<JSONObject>>::writeTo(AddressFloatSettings& n)
{
    n.min = m_obj["Min"].toDouble();
    n.max = m_obj["Max"].toDouble();
    n.unit = m_obj["Unit"].toString();
    n.clipMode = m_obj["ClipMode"].toString();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressIntSettings& n)
{
    m_stream << n.min << n.max << n.unit << n.clipMode;
    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressIntSettings& n)
{
    m_stream >> n.min >> n.max >> n.unit >> n.clipMode;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const AddressIntSettings& n)
{
    m_obj["Min"] = n.min;
    m_obj["Max"] = n.max;
    m_obj["Unit"] = n.unit;
    m_obj["ClipMode"] = n.clipMode;
}
template<>
void Visitor<Writer<JSONObject>>::writeTo(AddressIntSettings& n)
{
    n.min = m_obj["Min"].toInt();
    n.max = m_obj["Max"].toInt();
    n.unit = m_obj["Unit"].toString();
    n.clipMode = m_obj["ClipMode"].toString();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressSettings& n)
{
    m_stream << n.name
             << (int)n.ioType
             << n.priority
             << n.tags
             << n.value;
    if(n.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        readFrom(n.addressSpecificSettings.value<AddressFloatSettings>());
    }
    else if(n.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        readFrom(n.addressSpecificSettings.value<AddressIntSettings>());
    }

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressSettings& n)
{
    int ioType;
    m_stream >> n.name
            >> ioType
            >> n.priority
            >> n.tags
            >> n.value;

    n.ioType = static_cast<IOType>(ioType);

    if(n.value.type() == QMetaType::Float)
    {
        AddressFloatSettings s;
        writeTo(s);
        n.addressSpecificSettings = QVariant::fromValue(s);
    }
    else if(n.value.type() == QMetaType::Int)
    {
        AddressIntSettings s;
        writeTo(s);
        n.addressSpecificSettings = QVariant::fromValue(s);
    }

    checkDelimiter();
}
template<>
void Visitor<Reader<JSONObject>>::readFrom(const AddressSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["ioType"] = IOTypeStringMap()[n.ioType];
    m_obj["Priority"] = n.priority;
    m_obj["Tags"] = n.tags;
    m_obj["Type"] = QString::fromStdString(n.value.typeName());
    if(n.value.type() == QMetaType::Float)
    {
        m_obj["Value"] = n.value.toFloat();
        m_obj["AddressSpecificSettings"] = toJsonObject(n.addressSpecificSettings.value<AddressFloatSettings>());
    }
    else if(n.value.type() == QMetaType::Int)
    {
        m_obj["Value"] = n.value.toInt();
        m_obj["AddressSpecificSettings"] = toJsonObject(n.addressSpecificSettings.value<AddressIntSettings>());
    }
    else if(n.value.type() == QMetaType::QString)
    {
        m_obj["Value"] = n.value.toString();
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AddressSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.ioType = IOTypeStringMap().key(m_obj["ioType"].toString());
    n.priority = m_obj["Priority"].toInt();
    n.tags = m_obj["Tags"].toString();
    auto valueType = m_obj["Type"].toString();

    if(valueType == QString(QVariant::typeToName(QMetaType::Float)))
    {
        n.value = QVariant::fromValue(m_obj["Value"].toDouble());
        n.addressSpecificSettings = QVariant::fromValue(fromJsonObject<AddressFloatSettings>(m_obj["AddressSpecificSettings"].toObject()));
    }
    else if(valueType == QString(QVariant::typeToName(QMetaType::Int)))
    {
        n.value = QVariant::fromValue(m_obj["Value"].toInt());
        n.addressSpecificSettings = QVariant::fromValue(fromJsonObject<AddressIntSettings>(m_obj["AddressSpecificSettings"].toObject()));
    }
    else if(valueType == QString(QVariant::typeToName(QMetaType::QString)))
    {
        n.value = m_obj["Value"].toString();
    }
}
