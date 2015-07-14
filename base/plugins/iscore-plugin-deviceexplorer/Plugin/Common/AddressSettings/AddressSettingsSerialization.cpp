#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "AddressSettings.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Domain& n)
{
    m_stream << n.min << n.max << n.values;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Domain& n)
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



#include <boost/fusion/container/map.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/fusion/container/map/map_fwd.hpp>
#include <boost/fusion/include/map_fwd.hpp>
#include <boost/fusion/container/generation/make_map.hpp>
#include <boost/fusion/include/make_map.hpp>
#include <boost/fusion/include/at_key.hpp>


static QJsonArray convertQVariantList(const QVariantList& v)
{
    QJsonArray arr;
    for(const QVariant& elt : v)
    {
        switch(QMetaType::Type(elt.type()))
        {
            case QMetaType::Float:
            {
                arr.append(elt.toFloat());
                break;
            }
            case QMetaType::Int:
            {
                arr.append(elt.toInt());
                break;
            }
            case QMetaType::QString:
            {
                arr.append(elt.toString());
                break;
            }
            case QMetaType::Bool:
            {
                arr.append(elt.toBool());
                break;
            }
            case QMetaType::Char:
            {
                arr.append((char)elt.toInt());
                break;
            }
            case QMetaType::QVariantList:
            {
                arr.append(convertQVariantList(elt.toList()));
                break;
            }
            default:
                break;
        }
    }

    return arr;
}

static const auto QVariantConversionMap(
        boost::fusion::make_map<int,float,bool,QString, char, QVariantList>(
            [] (const QVariant& v) { return v.toInt(); },
            [] (const QVariant& v) { return v.toFloat(); },
            [] (const QVariant& v) { return v.toBool(); },
            [] (const QVariant& v) { return v.toString(); },
            [] (const QVariant& v) { return (char) v.toInt(); },
            [] (const QVariant& v) { return convertQVariantList(v.toList()); }
     )
);

template<typename T>
QJsonObject domainToJSON(const iscore::Domain& d)
{
    const auto& convert = boost::fusion::at_key<T>(QVariantConversionMap);
    QJsonObject obj;
    if(d.min.isValid())
        obj["Min"] = convert(d.min);
    if(d.max.isValid())
        obj["Max"] = convert(d.max);

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(convert(val));
    obj["Values"] = arr;

    return obj;
}

template<typename T>
iscore::Domain JSONToDomain(const QJsonObject& d);
template<>
iscore::Domain JSONToDomain<int>(const QJsonObject& obj)
{
    iscore::Domain d;
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
iscore::Domain JSONToDomain<float>(const QJsonObject& obj)
{
    iscore::Domain d;
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
iscore::Domain JSONToDomain<QString>(const QJsonObject& obj)
{
    iscore::Domain d;
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
    auto type = n.value.val.typeName();
    if(type)
    {
        m_obj["Type"] = QString::fromStdString(type);
        switch(QMetaType::Type(n.value.val.type()))
        {
            case QMetaType::Float:
            {
                m_obj["Value"] = n.value.val.toFloat();
                m_obj["Domain"] = domainToJSON<float>(n.domain);
                break;
            }
            case QMetaType::Int:
            {
                m_obj["Value"] = n.value.val.toInt();
                m_obj["Domain"] = domainToJSON<int>(n.domain);
                break;
            }
            case QMetaType::QString:
            {
                m_obj["Value"] = n.value.val.toString();
                m_obj["Domain"] = domainToJSON<QString>(n.domain);
                break;
            }
            case QMetaType::Bool:
            {
                m_obj["Value"] = n.value.val.toBool();
                m_obj["Domain"] = domainToJSON<bool>(n.domain);
                break;
            }
            case QMetaType::Char:
            {
                /*
                m_obj["Value"] = n.value.toChar();
                m_obj["Domain"] = domainToJSON<char>(n.domain);
                */
                break;
            }
            case QMetaType::QVariantList:
            {
                /*
                m_obj["Value"] = n.value.toBool();
                m_obj["Domain"] = domainToJSON<char>(n.domain);
                */
                break;
            }
            default:
                ISCORE_TODO;
                break;
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

    auto arr = m_obj["Tags"].toArray();
    for(auto&& elt : arr)
        n.tags.append(elt.toString());

    if(m_obj.contains("Type"))
    {
        auto valueType = m_obj["Type"].toString();

        if(valueType == QString(QVariant::typeToName(QMetaType::Float)))
        {
            n.value.val = QVariant::fromValue(m_obj["Value"].toDouble());
            n.domain = JSONToDomain<float>(m_obj["Domain"].toObject());
        }
        else if(valueType == QString(QVariant::typeToName(QMetaType::Int)))
        {
            n.value.val = QVariant::fromValue(m_obj["Value"].toInt());
            n.domain = JSONToDomain<int>(m_obj["Domain"].toObject());
        }
        else if(valueType == QString(QVariant::typeToName(QMetaType::QString)))
        {
            n.value.val = m_obj["Value"].toString();
            n.domain = JSONToDomain<QString>(m_obj["Domain"].toObject());
        }
        else if(valueType == QString(QVariant::typeToName(QMetaType::QString)))
        {
            n.value.val = m_obj["Value"].toString();
            n.domain = JSONToDomain<QString>(m_obj["Domain"].toObject());
        }
    }
}
