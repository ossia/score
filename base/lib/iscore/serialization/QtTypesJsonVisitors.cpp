#include <boost/none_t.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QPoint>
#include <array>
#include <QTransform>

#include <iscore/tools/std/Optional.hpp>
#include "DataStreamVisitor.hpp"
#include "JSONValueVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

auto makeJsonArray(std::initializer_list<QJsonValue> lst);
auto makeJsonArray(std::initializer_list<QJsonValue> lst)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
    QJsonArray arr;
    for(auto val : lst)
    {
        arr.append(val);
    }
    return arr;
#else
    return QJsonArray(lst);
#endif
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const QPointF& pt)
{
    val = makeJsonArray({pt.x(), pt.y()});
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(QPointF& pt)
{
    auto arr = val.toArray();
    pt.setX(arr[0].toDouble());
    pt.setY(arr[1].toDouble());
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(const QTransform& pt)
{
    val = makeJsonArray({
        pt.m11(), pt.m12(), pt.m13(),
        pt.m21(), pt.m22(), pt.m23(),
        pt.m31(), pt.m32(), pt.m33()});
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(QTransform& pt)
{
    auto arr = val.toArray();
    pt.setMatrix(
          arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble(),
          arr[3].toDouble(), arr[4].toDouble(), arr[5].toDouble(),
          arr[6].toDouble(), arr[7].toDouble(), arr[8].toDouble());
}


template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const std::array<float, 2>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream << obj[i];
    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(std::array<float, 2>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream >> obj[i];
    checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const std::array<float, 3>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream << obj[i];
    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(std::array<float, 3>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream >> obj[i];
    checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const std::array<float, 4>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream << obj[i];
    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(std::array<float, 4>& obj)
{
    for(auto i = 0U; i < obj.size(); i++)
        m_stream >> obj[i];
    checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const QJsonObject& obj)
{
    QJsonDocument doc{obj};
    m_stream << doc.toBinaryData();
    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(QJsonObject& obj)
{
    QByteArray arr;
    m_stream >> arr;

    obj = QJsonDocument::fromBinaryData(arr).object();

    checkDelimiter();
}

