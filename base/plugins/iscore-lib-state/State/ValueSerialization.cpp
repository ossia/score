#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Value.hpp"

#include <boost/fusion/container/map.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/fusion/container/map/map_fwd.hpp>
#include <boost/fusion/include/map_fwd.hpp>
#include <boost/fusion/container/generation/make_map.hpp>
#include <boost/fusion/include/make_map.hpp>
#include <boost/fusion/include/at_key.hpp>

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Value& value)
{
    m_stream << value.val;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Value& value)
{
    m_stream >> value.val;
    checkDelimiter();
}


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





QJsonValue ValueToJson(const iscore::Value & value)
{
    return QMetaType_QVariantToQJSonValue.at(QMetaType::Type(value.val.type()))(value.val);
}
iscore::Value JsonToValue(const QJsonValue &val, QMetaType::Type t)
{
    return iscore::Value::fromVariant(QMetaType_QJSonValueToQVariant.at(t)(val));
}


