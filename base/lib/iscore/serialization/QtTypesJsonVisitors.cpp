#include <boost/none_t.hpp>
#include <boost/optional/optional.hpp>
#include <QJsonArray>
#include <QJsonValue>
#include <QPoint>
#include <QTransform>

#include <iscore/tools/std/Optional.hpp>
#include "DataStreamVisitor.hpp"
#include "JSONValueVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

// TODO RENAME FILE
template<>
void Visitor<Reader<JSONValue>>::readFrom(const QPointF& pt)
{
    val = QJsonArray{pt.x(), pt.y()};
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(QPointF& pt)
{
    auto arr = val.toArray();
    pt.setX(arr[0].toDouble());
    pt.setY(arr[1].toDouble());
}

// TODO RENAME FILE
template<>
void Visitor<Reader<JSONValue>>::readFrom(const QTransform& pt)
{
    val = QJsonArray{
        pt.m11(), pt.m12(), pt.m13(),
        pt.m21(), pt.m22(), pt.m23(),
        pt.m31(), pt.m32(), pt.m33()};
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(QTransform& pt)
{
    auto arr = val.toArray();
    pt.setMatrix(
          arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble(),
          arr[3].toDouble(), arr[4].toDouble(), arr[5].toDouble(),
          arr[6].toDouble(), arr[7].toDouble(), arr[8].toDouble());
}
