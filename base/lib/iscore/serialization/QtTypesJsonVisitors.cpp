#include <boost/none_t.hpp>
#include <boost/optional/optional.hpp>
#include <QJsonArray>
#include <QJsonValue>
#include <QPoint>

#include <iscore/tools/std/Optional.hpp>
#include "DataStreamVisitor.hpp"
#include "JSONValueVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

// TODO RENAME FILE
template<>
void Visitor<Reader<JSONValue>>::readFrom(const QPointF& pt)
{
    QJsonArray arr;
    arr.append(pt.x());
    arr.append(pt.y());

    val = arr;
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(QPointF& pt)
{
    auto arr = val.toArray();
    pt.setX(arr[0].toDouble());
    pt.setY(arr[1].toDouble());
}
