#include "FullViewConstraintViewModel.hpp"

#include "Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
    m_stream << constraint.zoom() << constraint.center();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
    m_obj["Zoom"] = constraint.zoom();
    m_obj["CenterOn"] = toJsonValue(constraint.center());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(FullViewConstraintViewModel& cvm)
{
    double z;
    QPointF c;
    m_stream >> z >> c;
    cvm.setCenter(c);
    cvm.setZoom(z);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(FullViewConstraintViewModel& cvm)
{
    auto z = m_obj["Zoom"].toDouble();
    auto c = m_obj["CenterOn"].toArray();
    QPointF center { c.at(0).toDouble(), c.at(1).toDouble() };

    cvm.setCenter(center);
    cvm.setZoom(z);
}
