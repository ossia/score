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


template<typename T>
QJsonObject domainToJSON(const iscore::Domain& d);

template<typename T>
iscore::Domain JSONToDomain(const QJsonObject& obj);

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

static QVariantList convertJSonValueList(const QJsonArray& v)
{
    QVariantList arr;
    for(const QJsonValue& elt : v)
    {
        switch(elt.type())
        {
            case QJsonValue::Double:
            {
                arr.append(elt.toDouble());
                break;
            }
            case QJsonValue::String:
            {
                arr.append(elt.toString());
                break;
            }
            case QJsonValue::Bool:
            {
                arr.append(elt.toBool());
                break;
            }
                // TODO Char has to be handled outside!
            case QJsonValue::Array:
            {
                arr.append(convertJSonValueList(elt.toArray()));
                break;
            }
            default:
                break;
        }
    }

    return arr;
}


static const auto QVariantToJSonValue(
        boost::fusion::make_map<int,float,bool,QString, char, QVariantList>(
            [] (const QVariant& v) { return v.toInt(); },
            [] (const QVariant& v) { return v.toFloat(); },
            [] (const QVariant& v) { return v.toBool(); },
            [] (const QVariant& v) { return v.toString(); },
            [] (const QVariant& v) { return (char) v.toInt(); },
            [] (const QVariant& v) { return convertQVariantList(v.toList()); }
     )
);

static const auto JSonValueToQVariant(
        boost::fusion::make_map<int,float,bool,QString, char, QVariantList>(
            [] (const QJsonValue& v) { return v.toInt(); },
            [] (const QJsonValue& v) { return (float)v.toDouble(); },
            [] (const QJsonValue& v) { return v.toBool(); },
            [] (const QJsonValue& v) { return v.toString(); },
            [] (const QJsonValue& v) { return (char) v.toInt(); },
            [] (const QJsonValue& v) { return convertJSonValueList(v.toArray()); }
     )
);

static const std::map<
    QMetaType::Type,
    std::function<QJsonValue(const QVariant&)>>
        QMetaType_QVariantToQJSonValue
{
    {QMetaType::Int, boost::fusion::at_key<int>(QVariantToJSonValue)},
{QMetaType::Float, boost::fusion::at_key<float>(QVariantToJSonValue)},
{QMetaType::Bool, boost::fusion::at_key<bool>(QVariantToJSonValue)},
{QMetaType::QString, boost::fusion::at_key<QString>(QVariantToJSonValue)},
{QMetaType::Char, boost::fusion::at_key<char>(QVariantToJSonValue)},
{QMetaType::QVariantList, boost::fusion::at_key<QVariantList>(QVariantToJSonValue)}
};

static const std::map<
    QMetaType::Type,
    std::function<QJsonObject(const iscore::Domain&)>>
        QMetaType_DomainToJson
{
{QMetaType::Int, [] (const iscore::Domain& v) { return domainToJSON<int>(v); }} ,
{QMetaType::Float, [] (const iscore::Domain& v) { return domainToJSON<float>(v); }} ,
{QMetaType::Bool, [] (const iscore::Domain& v) { return domainToJSON<bool>(v); }} ,
{QMetaType::QString, [] (const iscore::Domain& v) { return domainToJSON<QString>(v); }} ,
{QMetaType::Char, [] (const iscore::Domain& v) { return domainToJSON<char>(v); }} ,
{QMetaType::QVariantList, [] (const iscore::Domain& v) { return domainToJSON<QVariantList>(v); }}
};


static const std::map<
    QMetaType::Type,
    std::function<QVariant(const QJsonValue&)>>
        QMetaType_QJSonValueToQVariant
{
    {QMetaType::Int, boost::fusion::at_key<int>(JSonValueToQVariant)},
{QMetaType::Float, boost::fusion::at_key<float>(JSonValueToQVariant)},
{QMetaType::Bool, boost::fusion::at_key<bool>(JSonValueToQVariant)},
{QMetaType::QString, boost::fusion::at_key<QString>(JSonValueToQVariant)},
{QMetaType::Char, boost::fusion::at_key<char>(JSonValueToQVariant)},
{QMetaType::QVariantList, boost::fusion::at_key<QVariantList>(JSonValueToQVariant)}
};


static const std::map<
    QMetaType::Type,
    std::function<iscore::Domain(const QJsonObject&)>>
        QMetaType_JsonToDomain
{
{QMetaType::Int, [] (const QJsonObject& v) { return JSONToDomain<int>(v); }} ,
{QMetaType::Float, [] (const QJsonObject& v) { return JSONToDomain<float>(v); }} ,
{QMetaType::Bool, [] (const QJsonObject& v) { return JSONToDomain<bool>(v); }} ,
{QMetaType::QString, [] (const QJsonObject& v) { return JSONToDomain<QString>(v); }} ,
{QMetaType::Char, [] (const QJsonObject& v) { return JSONToDomain<char>(v); }} ,
{QMetaType::QVariantList, [] (const QJsonObject& v) { return JSONToDomain<QVariantList>(v); }}
};

template<typename T>
QJsonObject domainToJSON(const iscore::Domain& d)
{
    const auto& convert = boost::fusion::at_key<T>(QVariantToJSonValue);
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
iscore::Domain JSONToDomain(const QJsonObject& obj)
{
    const auto& convert = boost::fusion::at_key<T>(JSonValueToQVariant);
    iscore::Domain d;
    if(obj.contains("Min"))
    {
        d.min = convert(obj["Min"]);
    }
    if(obj.contains("Max"))
    {
        d.max = convert(obj["Max"]);
    }

    QVariantList arr;
    for(const QJsonValue& val : obj["Values"].toArray())
        arr.append(convert(val));

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

        auto t = QMetaType::Type(n.value.val.type());
        m_obj["Value"] = QMetaType_QVariantToQJSonValue.at(t)(n.value.val);
        m_obj["Domain"] = QMetaType_DomainToJson.at(t)(n.domain);
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
        QMetaType::Type valueType = QMetaType::Type(QMetaType::type(m_obj["Type"].toString().toLatin1()));
        n.value.val = QMetaType_QJSonValueToQVariant.at(valueType)(m_obj["Value"]);
        n.domain = QMetaType_JsonToDomain.at(valueType)(m_obj["Domain"].toObject());
    }
}
