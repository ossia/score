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
        switch(static_cast<QMetaType::Type>(elt.type()))
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
            case QMetaType::QChar:
            {
                arr.append(elt.toString()); // TODO here we loose the "char" information...
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
                arr.append(elt.toString()); // Here the string may be a char.
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
                ISCORE_TODO;
                // TODO invalid case;
                break;
        }
    }

    return arr;
}

struct invalid_t {};
// Note : chars aren't properly handled here.
static const auto QVariantToJSonValue(
        boost::fusion::make_map<invalid_t, int,float,bool,QString, QChar, QVariantList>(
            [] (const QVariant& v) { return QJsonValue{}; },
            [] (const QVariant& v) { return v.toInt(); },
            [] (const QVariant& v) { return v.toFloat(); },
            [] (const QVariant& v) { return v.toBool(); },
            [] (const QVariant& v) { return v.toString(); },
            [] (const QVariant& v) { return v.toString(); },
            [] (const QVariant& v) { return convertQVariantList(v.toList()); }
     )
);

static const auto JSonValueToQVariant(
        boost::fusion::make_map<invalid_t, int,float,bool,QString, QChar, QVariantList>(
            [] (const QJsonValue& v) { return QVariant{}; },
            [] (const QJsonValue& v) { return v.toInt(); },
            [] (const QJsonValue& v) { return (float)v.toDouble(); },
            [] (const QJsonValue& v) { return v.toBool(); },
            [] (const QJsonValue& v) { return v.toString(); },
            [] (const QJsonValue& v) { return v.toString().at(0); },
            [] (const QJsonValue& v) { return convertJSonValueList(v.toArray()); }
     )
);

static const std::map<
    QMetaType::Type,
    std::function<QJsonValue(const QVariant&)>>
        QMetaType_QVariantToQJSonValue
{
{QMetaType::UnknownType, boost::fusion::at_key<invalid_t>(QVariantToJSonValue)},
{QMetaType::Int, boost::fusion::at_key<int>(QVariantToJSonValue)},
{QMetaType::Float, boost::fusion::at_key<float>(QVariantToJSonValue)},
{QMetaType::Bool, boost::fusion::at_key<bool>(QVariantToJSonValue)},
{QMetaType::QString, boost::fusion::at_key<QString>(QVariantToJSonValue)},
{QMetaType::QChar, boost::fusion::at_key<QChar>(QVariantToJSonValue)},
{QMetaType::QVariantList, boost::fusion::at_key<QVariantList>(QVariantToJSonValue)}
};


static const std::map<
    QMetaType::Type,
    std::function<QVariant(const QJsonValue&)>>
        QMetaType_QJSonValueToQVariant
{
{QMetaType::UnknownType, boost::fusion::at_key<invalid_t>(JSonValueToQVariant)},
{QMetaType::Int, boost::fusion::at_key<int>(JSonValueToQVariant)},
{QMetaType::Float, boost::fusion::at_key<float>(JSonValueToQVariant)},
{QMetaType::Bool, boost::fusion::at_key<bool>(JSonValueToQVariant)},
{QMetaType::QString, boost::fusion::at_key<QString>(JSonValueToQVariant)},
{QMetaType::QChar, boost::fusion::at_key<QChar>(JSonValueToQVariant)},
{QMetaType::QVariantList, boost::fusion::at_key<QVariantList>(JSonValueToQVariant)}
};





QJsonValue ValueToJson(const iscore::Value & value)
{
    return QMetaType_QVariantToQJSonValue.at(static_cast<QMetaType::Type>(value.val.type()))(value.val);
}
iscore::Value JsonToValue(const QJsonValue &val, QMetaType::Type t)
{
    return iscore::Value::fromVariant(QMetaType_QJSonValueToQVariant.at(t)(val));
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::Value& val)
{
    auto type = val.val.typeName();
    if(type)
    {
        m_obj["Type"] = QString::fromStdString(type);
        m_obj["Value"] = ValueToJson(val);
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::Value& val)
{
    if(m_obj.contains("Type"))
    {
        auto valueType = static_cast<QMetaType::Type>(QMetaType::type(m_obj["Type"].toString().toUtf8()));
        val = JsonToValue(m_obj["Value"], valueType);
    }
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::OptionalValue& obj)
{
    m_stream << static_cast<bool>(obj);

    if(obj)
    {
        m_stream << *obj;
    }
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::OptionalValue& obj)
{
    bool b {};
    m_stream >> b;

    if(b)
    {
        iscore::Value val;
        m_stream >> val;

        obj = val;
    }
    else
    {
        obj = boost::none_t {};
    }
}
