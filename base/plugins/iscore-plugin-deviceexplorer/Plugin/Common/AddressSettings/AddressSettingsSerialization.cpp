#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "AddressSettings.hpp"
#include "AddressSpecificSettings/AddressFloatSettings.hpp"
#include "AddressSpecificSettings/AddressIntSettings.hpp"
#include "AddressSpecificSettings/AddressStringSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const Domain& n)
{
    m_stream << n.min << n.max << n.values;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Domain& n)
{
    m_stream >> n.min >> n.max >> n.values;
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const AddressSettings& n)
{
    m_stream << n.name
             << n.value
             << n.domain
             << (int)n.ioType
             << (int)n.clipMode
             << n.unit
             << n.repetitionFilter
             << n.rate
             << n.priority
             << n.tags;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(AddressSettings& n)
{
    m_stream >> n.name
            >> n.value
            >> n.domain
            >> (int&)n.ioType
            >> (int&)n.clipMode
            >> n.unit
            >> n.repetitionFilter
            >> n.rate
            >> n.priority
            >> n.tags;

    checkDelimiter();
}

template<typename T>
QJsonObject domainToJSON(const Domain& d);

template<>
QJsonObject domainToJSON<int>(const Domain& d)
{
    QJsonObject obj;
    if(d.min.isValid())
        obj["Min"] = d.min.toInt();
    if(d.max.isValid())
        obj["Max"] = d.max.toInt();

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(val.toInt());
    obj["Values"] = arr;

    return obj;
}

template<>
QJsonObject domainToJSON<float>(const Domain& d)
{
    QJsonObject obj;
    if(d.min.isValid())
        obj["Min"] = d.min.toFloat();
    if(d.max.isValid())
        obj["Max"] = d.max.toFloat();

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(val.toFloat());
    obj["Values"] = arr;

    return obj;
}

template<>
QJsonObject domainToJSON<QString>(const Domain& d)
{
    QJsonObject obj;
    if(d.min.isValid())
        obj["Min"] = d.min.toString();
    if(d.max.isValid())
        obj["Max"] = d.max.toString();

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(val.toString());
    obj["Values"] = arr;

    return obj;
}


template<typename T>
Domain JSONToDomain(const QJsonObject& d);
template<>
Domain JSONToDomain<int>(const QJsonObject& obj)
{
    Domain d;
    if(obj.contains("Min"))
    {
        d.min = obj["Min"].toInt();
    }
    if(obj.contains("Max"))
    {
        d.max = obj["Max"].toInt();
    }

    QVariantList arr;
    for(auto&& val : obj["Values"].toArray())
        arr.append(val.toInt());

    return d;
}

template<>
Domain JSONToDomain<float>(const QJsonObject& obj)
{
    Domain d;
    if(obj.contains("Min"))
    {
        d.min = (float)obj["Min"].toDouble();
    }
    if(obj.contains("Max"))
    {
        d.max = (float)obj["Max"].toDouble();
    }

    QVariantList arr;
    for(auto&& val : obj["Values"].toArray())
        arr.append((float)val.toDouble());

    return d;
}

template<>
Domain JSONToDomain<QString>(const QJsonObject& obj)
{
    Domain d;
    if(obj.contains("Min"))
    {
        d.min = obj["Min"].toString();
    }
    if(obj.contains("Max"))
    {
        d.max = obj["Max"].toString();
    }

    QVariantList arr;
    for(auto&& val : obj["Values"].toArray())
        arr.append(val.toString());

    return d;
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const AddressSettings& n)
{
    m_obj["Name"] = n.name;

    // Metadata
    m_obj["ioType"] = IOTypeStringMap()[n.ioType];
    m_obj["ClipMode"] = ClipModeStringMap()[n.clipMode];
    m_obj["Unit"] = n.unit;

    m_obj["RepetitionFilter"] = n.repetitionFilter;
    m_obj["RefreshRate"] = n.rate;

    m_obj["Priority"] = n.priority;

    QJsonArray arr;
    for(auto& str : n.tags)
        arr.append(str);
    m_obj["Tags"] = arr;

    // Value, domain and type
    auto type = n.value.typeName();
    if(type)
    {
        m_obj["Type"] = QString::fromStdString(type);
        if(n.value.type() == QMetaType::Float)
        {
            m_obj["Value"] = n.value.toFloat();
            m_obj["Domain"] = domainToJSON<float>(n.domain);
        }
        else if(n.value.type() == QMetaType::Int)
        {
            m_obj["Value"] = n.value.toInt();
            m_obj["Domain"] = domainToJSON<int>(n.domain);
        }
        else if(n.value.type() == QMetaType::QString)
        {
            m_obj["Value"] = n.value.toString();
            m_obj["Domain"] = domainToJSON<QString>(n.domain);
        }
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AddressSettings& n)
{
    n.name = m_obj["Name"].toString();

    n.ioType = IOTypeStringMap().key(m_obj["ioType"].toString());
    n.clipMode = ClipModeStringMap().key(m_obj["ClipMode"].toString());
    n.unit = m_obj["Unit"].toString();

    n.repetitionFilter = m_obj["RepetitionFilter"].toBool();
    n.rate = m_obj["RefreshRate"].toInt();

    n.priority = m_obj["Priority"].toInt();

    fromJsonArray(m_obj["Tags"].toArray(), n.tags);


    if(m_obj.contains("Type"))
    {
        auto valueType = m_obj["Type"].toString();

        if(valueType == QString(QVariant::typeToName(QMetaType::Float)))
        {
            n.value = QVariant::fromValue(m_obj["Value"].toDouble());
            n.domain = JSONToDomain<float>(m_obj["Domain"].toObject());
        }
        else if(valueType == QString(QVariant::typeToName(QMetaType::Int)))
        {
            n.value = QVariant::fromValue(m_obj["Value"].toInt());
            n.domain = JSONToDomain<int>(m_obj["Domain"].toObject());
        }
        else if(valueType == QString(QVariant::typeToName(QMetaType::QString)))
        {
            n.value = m_obj["Value"].toString();
            n.domain = JSONToDomain<QString>(m_obj["Domain"].toObject());
        }
    }

}
