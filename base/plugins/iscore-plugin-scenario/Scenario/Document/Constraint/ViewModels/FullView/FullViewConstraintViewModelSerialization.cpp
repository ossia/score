#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>

#include "FullViewConstraintViewModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Scenario::FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const Scenario::ConstraintViewModel&>(constraint));
    m_stream << constraint.zoom() << constraint.center();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Scenario::FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const Scenario::ConstraintViewModel&>(constraint));
    m_obj["Zoom"] = constraint.zoom();
    m_obj["CenterOn"] = toJsonValue(constraint.center());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Scenario::FullViewConstraintViewModel& cvm)
{
    double z;
    QPointF c;
    m_stream >> z >> c;
    cvm.setCenter(c);
    cvm.setZoom(z);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::FullViewConstraintViewModel& cvm)
{
    auto z = m_obj["Zoom"].toDouble();
    auto c = m_obj["CenterOn"].toArray();
    QPointF center { c.at(0).toDouble(), c.at(1).toDouble() };

    cvm.setCenter(center);
    cvm.setZoom(z);
}
