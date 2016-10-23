#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/VariantSerialization.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonValue>

#include "DomainSerialization.hpp"
#include <iscore_lib_device_export.h>
#include <State/Value.hpp>
JSON_METADATA(ossia::Impulse, "Impulse")
JSON_METADATA(ossia::Int, "Int")
JSON_METADATA(ossia::Char, "Char")
JSON_METADATA(ossia::Bool, "Bool")
JSON_METADATA(ossia::Float, "Float")
JSON_METADATA(ossia::Vec2f, "Vec2f")
JSON_METADATA(ossia::Vec3f, "Vec3f")
JSON_METADATA(ossia::Vec4f, "Vec4f")
JSON_METADATA(ossia::Tuple, "Tuple")
JSON_METADATA(ossia::String, "String")
JSON_METADATA(ossia::value, "Generic")
JSON_METADATA(ossia::net::domain_base<ossia::Impulse>, "Impulse")
JSON_METADATA(ossia::net::domain_base<ossia::Int>, "Int")
JSON_METADATA(ossia::net::domain_base<ossia::Char>, "Char")
JSON_METADATA(ossia::net::domain_base<ossia::Bool>, "Bool")
JSON_METADATA(ossia::net::domain_base<ossia::Float>, "Float")
JSON_METADATA(ossia::net::domain_base<ossia::Vec2f>, "Vec2f")
JSON_METADATA(ossia::net::domain_base<ossia::Vec3f>, "Vec3f")
JSON_METADATA(ossia::net::domain_base<ossia::Vec4f>, "Vec4f")
JSON_METADATA(ossia::net::domain_base<ossia::Tuple>, "Tuple")
JSON_METADATA(ossia::net::domain_base<ossia::String>, "String")
JSON_METADATA(ossia::net::domain_base<ossia::value>, "Generic")

ISCORE_DECL_VALUE_TYPE(int)
ISCORE_DECL_VALUE_TYPE(float)
ISCORE_DECL_VALUE_TYPE(bool)
ISCORE_DECL_VALUE_TYPE(char)
ISCORE_DECL_VALUE_TYPE(ossia::Impulse)
ISCORE_DECL_VALUE_TYPE(std::string)
ISCORE_DECL_VALUE_TYPE(ossia::Vec2f)
ISCORE_DECL_VALUE_TYPE(ossia::Vec3f)
ISCORE_DECL_VALUE_TYPE(ossia::Vec4f)

template<>
template<>
void VariantDataStreamSerializer<ossia::value::value_type>::perform<ossia::Destination>()
{
    return; // How could we serialize destination ?
}

template<>
template<>
void VariantDataStreamDeserializer<ossia::value::value_type>::perform<ossia::Destination>()
{
    i++;
    return; // How could we deserialize destination ?
}

template<>
template<>
void VariantJSONSerializer<ossia::value::value_type>::perform<ossia::Destination>()
{
    return; // How could we serialize destination ?
}

template<>
template<>
void VariantJSONDeserializer<ossia::value::value_type>::perform<ossia::Destination>()
{
    return; // How could we deserialize destination ?
}


template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const ossia::value& n)
{
    readFrom(n.v);
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<DataStream>>::writeTo(ossia::value& n)
{
    writeTo(n.v);
}

DataStreamInput& operator<< (DataStreamInput& stream, const ossia::value& obj)
{
    Visitor<Reader<DataStream>> reader{stream.stream.device()};
    reader.readFrom(obj);
    return stream;
}

DataStreamOutput& operator>> (DataStreamOutput& stream, ossia::value& obj)
{
    Visitor<Writer<DataStream>> writer{stream.stream.device()};
    writer.writeTo(obj);

    return stream;
}


template<typename T>
struct TSerializer<DataStream, void, ossia::net::domain_base<T>>
{
        using domain_t = ossia::net::domain_base<T>;
        static void readFrom(
                DataStream::Serializer& s,
                const domain_t& domain)
        {
            // Min
            {
                bool min_b = domain.min;
                s.stream() << min_b;
                if(min_b)
                    s.stream() << domain.min.get();
            }

            // Max
            {
                bool max_b = domain.max;
                s.stream() << max_b;
                if(max_b)
                    s.stream() << domain.max.get();
            }

            // Values
            {
                s.stream() << (int32_t) domain.values.size();
                for(auto& val : domain.values)
                    s.stream() << val;
            }

            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                domain_t& domain)
        {
            {
                bool min_b;
                s.stream() >> min_b;
                if(min_b)
                {
                    typename domain_t::value_type v;
                    s.stream() >> v;
                    domain.min = std::move(v);
                }
            }

            {
                bool max_b;
                s.stream() >> max_b;
                if(max_b)
                {
                    typename domain_t::value_type v;
                    s.stream() >> v;
                    domain.max = std::move(v);
                }
            }

            {
                int32_t count;
                s.stream() >> count;
                for(int i = 0; i < count; i++)
                {
                    typename domain_t::value_type v;
                    s.stream() >> v;
                    domain.values.insert(v);
                }
            }

            s.checkDelimiter();
        }
};

template<>
struct TSerializer<DataStream, void, ossia::net::domain_base<std::string>>
{
        using domain_t = ossia::net::domain_base<std::string>;
        static void readFrom(
                DataStream::Serializer& s,
                const domain_t& domain)
        {
            // Values
            {
                s.stream() << (int32_t) domain.values.size();
                for(auto& val : domain.values)
                    s.stream() << val;
            }

            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                domain_t& domain)
        {
            {
                int32_t count;
                s.stream() >> count;
                for(int i = 0; i < count; i++)
                {
                    std::string v;
                    s.stream() >> v;
                    domain.values.insert(v);
                }
            }

            s.checkDelimiter();
        }
};

template<>
struct TSerializer<DataStream, void, ossia::net::domain_base<ossia::Impulse>>
{
        using domain_t = ossia::net::domain_base<ossia::Impulse>;
        static void readFrom(
                DataStream::Serializer& s,
                const domain_t& domain)
        {
            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                domain_t& domain)
        {
            s.checkDelimiter();
        }
};

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const ossia::net::domain& n)
{
    m_stream << (const ossia::net::domain_base_variant&)n;
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<DataStream>>::writeTo(ossia::net::domain& n)
{
    m_stream >> (ossia::net::domain_base_variant&)n;
}



/// JSON ///

QJsonValue toJsonValue(int obj)
{ return obj; }
QJsonValue toJsonValue(float obj)
{ return obj; }
QJsonValue toJsonValue(char obj)
{ return QString(QChar(obj)); }
QJsonValue toJsonValue(bool obj)
{ return obj; }
template<>
QJsonValue toJsonValue<int>(const int& obj)
{ return obj; }
template<>
QJsonValue toJsonValue<float>(const float& obj)
{ return obj; }
template<>
QJsonValue toJsonValue<char>(const char& obj)
{ return QString(QChar(obj)); }
template<>
QJsonValue toJsonValue<bool>(const bool& obj)
{ return obj; }
template<>
QJsonValue toJsonValue<std::array<float, 2>>(const std::array<float, 2>& obj)
{
    QJsonArray arr;
    for(std::size_t i = 0; i < 2; i++)
        arr.push_back(obj[i]);
    return arr;
}
template<>
QJsonValue toJsonValue<std::array<float, 3>>(const std::array<float, 3>& obj)
{
    QJsonArray arr;
    for(std::size_t i = 0; i < 3; i++)
        arr.push_back(obj[i]);
    return arr;
}
template<>
QJsonValue toJsonValue<std::array<float, 4>>(const std::array<float, 4>& obj)
{
    QJsonArray arr;
    for(std::size_t i = 0; i < 4; i++)
        arr.push_back(obj[i]);
    return arr;
}
QJsonValue toJsonValue(const std::string& obj)
{
    return QString::fromStdString(obj);
}

template<>
std::string fromJsonValue<std::string>(const QJsonValue& obj)
{
    return obj.toString().toStdString();
}
template<>
int fromJsonValue<int>(const QJsonValue& obj)
{
    return obj.toInt();
}
template<>
float fromJsonValue<float>(const QJsonValue& obj)
{
    return obj.toDouble();
}
template<>
char fromJsonValue<char>(const QJsonValue& obj)
{
    auto s = obj.toString();
    return s.isEmpty() ? (char)0 : s[0].toLatin1();
}
template<>
bool fromJsonValue<bool>(const QJsonValue& obj)
{
    return obj.toBool();
}

template<>
std::string fromJsonValue<std::string>(const QJsonValueRef& obj)
{
    return obj.toString().toStdString();
}

template<>
int fromJsonValue<int>(const QJsonValueRef& obj)
{
    return obj.toInt();
}
template<>
float fromJsonValue<float>(const QJsonValueRef& obj)
{
    return obj.toDouble();
}
template<>
char fromJsonValue<char>(const QJsonValueRef& obj)
{
    auto s = obj.toString();
    return s.isEmpty() ? (char)0 : s[0].toLatin1();
}
template<>
bool fromJsonValue<bool>(const QJsonValueRef& obj)
{
    return obj.toBool();
}
template<>
ossia::Impulse fromJsonValue<ossia::Impulse>(const QJsonValueRef& obj)
{
    return {};
}
template<>
std::array<float, 2> fromJsonValue<std::array<float, 2>>(const QJsonValueRef& obj)
{
    std::array<float, 2> v;
    auto arr = obj.toArray();
    const std::size_t N = std::min(2, arr.size());
    for(std::size_t i = 0; i < N; i++)
        v[i] = arr[i].toDouble();

    return v;
}
template<>
std::array<float, 3> fromJsonValue<std::array<float, 3>>(const QJsonValueRef& obj)
{
    std::array<float, 3> v;
    auto arr = obj.toArray();
    const std::size_t N = std::min(3, arr.size());
    for(std::size_t i = 0; i < N; i++)
        v[i] = arr[i].toDouble();

    return v;
}
template<>
std::array<float, 4> fromJsonValue<std::array<float, 4>>(const QJsonValueRef& obj)
{
    std::array<float, 4> v;
    auto arr = obj.toArray();
    const std::size_t N = std::min(4, arr.size());
    for(std::size_t i = 0; i < N; i++)
        v[i] = arr[i].toDouble();

    return v;
}
template<typename T>
QJsonArray toJsonArray(const boost::container::flat_set<T>& array)
{
    QJsonArray arr;
    for(auto& v : array)
        arr.push_back(toJsonValue(v));
    return arr;
}

QJsonArray toJsonArray(const std::vector<ossia::value>& array)
{
    QJsonArray arr;
    for(auto& v : array)
        arr.push_back(toJsonValue(v));
    return arr;
}

void fromJsonArray(const QJsonArray& arr, std::vector<ossia::value>& array)
{
    array.reserve(arr.size());
    for(const auto& val : arr)
    {
        array.push_back(fromJsonValue<ossia::value>(val));
    }
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const ossia::value& n);
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(ossia::value& n);
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const ossia::value& n);
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(ossia::value& n);

template<>
struct TSerializer<JSONObject, std::vector<ossia::value>>
{
    static void readFrom(
            JSONObject::Serializer& s,
            const std::vector<ossia::value>& vec)
    {
        s.m_obj[s.strings.Values] = toJsonArray(vec);
    }

    static void writeTo(
            JSONObject::Deserializer& s,
            std::vector<ossia::value>& vec)
    {
        fromJsonArray(s.m_obj[s.strings.Values].toArray(), vec);
    }
};

template<>
struct TSerializer<JSONValue, std::vector<ossia::value>>
{
    static void readFrom(
            JSONValue::Serializer& s,
            const std::vector<ossia::value>& vec)
    {
        s.val = toJsonArray(vec);
    }

    static void writeTo(
            JSONValue::Deserializer& s,
            std::vector<ossia::value>& vec)
    {
        fromJsonArray(s.val.toArray(), vec);
    }
};

template<>
struct TSerializer<JSONValue, std::string>
{
    static void readFrom(
            JSONValue::Serializer& s,
            const std::string& v)
    {
        s.val = QString::fromStdString(v);
    }

    static void writeTo(
            JSONValue::Deserializer& s,
            std::string& val)
    {
        val = s.val.toString().toStdString();
    }
};

template<typename T>
struct TSerializer<JSONObject, ossia::net::domain_base<T>>
{
        using domain_t = ossia::net::domain_base<T>;
        static void readFrom(
                JSONObject::Serializer& s,
                const domain_t& domain)
        {
            if(domain.min)
                s.m_obj[s.strings.Min] = toJsonValue(domain.min.get());
            if(domain.max)
                s.m_obj[s.strings.Min] = toJsonValue(domain.max.get());
            if(!domain.values.empty())
                s.m_obj[s.strings.Values] = toJsonArray(domain.values);
        }

        static void writeTo(
                const JSONObject::Deserializer& s,
                domain_t& domain)
        {
            using val_t = typename domain_t::value_type;
            // OPTIMIZEME there should be something in boost
            // to get multiple iterators from multiple keys in one pass...
            auto it_min = s.m_obj.constFind(s.strings.Min);
            auto it_max = s.m_obj.constFind(s.strings.Max);
            auto it_values = s.m_obj.constFind(s.strings.Values);
            if(it_min != s.m_obj.constEnd())
            {
                domain.min = fromJsonValue<val_t>(*it_min);
            }
            if(it_max != s.m_obj.constEnd())
            {
                domain.max = fromJsonValue<val_t>(*it_max);
            }
            if(it_values != s.m_obj.constEnd())
            {
                const auto arr = it_values->toArray();
                for(const auto& v : arr)
                {
                    domain.values.insert(fromJsonValue<val_t>(v));
                }
            }
        }
};


template<>
struct TSerializer<JSONObject, ossia::net::domain_base<std::string>>
{
        using domain_t = ossia::net::domain_base<std::string>;
        static void readFrom(
                JSONObject::Serializer& s,
                const domain_t& domain)
        {
            if(!domain.values.empty())
                s.m_obj[s.strings.Values] = toJsonArray(domain.values);
        }

        static void writeTo(
                JSONObject::Deserializer& s,
                domain_t& domain)
        {
            auto it_values = s.m_obj.constFind(s.strings.Values);
            if(it_values != s.m_obj.constEnd())
            {
                const auto arr = it_values->toArray();
                for(const auto& v : arr)
                {
                    domain.values.insert(fromJsonValue<std::string>(v));
                }
            }
        }
};

template<>
struct TSerializer<JSONObject, ossia::net::domain_base<ossia::Impulse>>
{
        using domain_t = ossia::net::domain_base<ossia::Impulse>;
        static void readFrom(
                JSONObject::Serializer& s,
                const domain_t& domain)
        {
        }

        static void writeTo(
                JSONObject::Deserializer& s,
                domain_t& domain)
        {
        }
};

namespace Device
{


QJsonObject DomainToJson(const ossia::net::domain& d)
{
    return marshall<JSONObject>((const ossia::net::domain_base_variant&) d);
    /*
    QJsonObject obj;
    auto& strings = iscore::StringConstant();

    obj[strings.Min] = ValueToJson(d.min);
    obj[strings.Max] = ValueToJson(d.max);

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(ValueToJson(val));
    obj[strings.Values] = arr;

    return obj;
    */
}

ossia::net::domain JsonToDomain(const QJsonObject& obj, const QString& t)
{
    // TODO optimize me
    ossia::net::domain dom;
    Deserializer<JSONObject> d{obj};
    d.writeTo((ossia::net::domain_base_variant&) dom);
    return dom;


    /*
    Device::Domain d;
    auto& strings = iscore::StringConstant();

    auto min_it = obj.constFind(strings.Min);
    if(min_it != obj.constEnd())
    {
        d.min = State::convert::fromQJsonValue(*min_it, t);
    }

    auto max_it = obj.constFind(strings.Max);
    if(max_it != obj.constEnd())
    {
        d.max = State::convert::fromQJsonValue(*max_it, t);
    }

    for(const QJsonValue& val : obj[strings.Values].toArray())
        d.values.append(State::convert::fromQJsonValue(val, t));

    return d;
    */
}
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const ossia::value& n)
{
    readFrom(n.v);
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(ossia::value& n)
{
    writeTo(n.v);
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const ossia::value& n)
{
    val = marshall<JSONObject>(n);
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(ossia::value& n)
{
    n = unmarshall<ossia::value>(val.toObject());
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const ossia::Impulse& n)
{
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(ossia::Impulse& n)
{
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const std::array<float, 2>& n)
{
    val = toJsonValue(n);
}
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const std::array<float, 3>& n)
{
    val = toJsonValue(n);
}
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const std::array<float, 4>& n)
{
    val = toJsonValue(n);
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(std::array<float, 2>& n)
{
    fromJsonValue(val, n);
}
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(std::array<float, 3>& n)
{
    fromJsonValue(val, n);
}
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(std::array<float, 4>& n)
{
    fromJsonValue(val, n);
}

