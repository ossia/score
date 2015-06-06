#include "JSONValueVisitor.hpp"
#include <QPointF>

template<>
void Visitor<Reader<JSONValue>>::readFrom(const QPointF& pt)
{
    QJsonArray arr{pt.x(), pt.y()};
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
