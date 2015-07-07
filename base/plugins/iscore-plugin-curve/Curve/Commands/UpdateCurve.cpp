#include "UpdateCurve.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"

UpdateCurve::UpdateCurve(ObjectPath&& model, QVector<QByteArray> &&segments):
    iscore::SerializableCommand{
        "AutomationControl", commandName(), description()},
    m_model{std::move(model)},
    m_newCurveData{std::move(segments)}
{
    const auto& curve = m_model.find<CurveModel>();
    for(const auto& segment : curve.segments())
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        m_oldCurveData.append(arr);
    }
}

void UpdateCurve::undo()
{
    auto& curve = m_model.find<CurveModel>();
    curve.clear();

    for(const auto& elt : m_oldCurveData)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }

    curve.changed();
}

void UpdateCurve::redo()
{
    auto& curve = m_model.find<CurveModel>();
    curve.clear();

    for(const auto& elt : m_newCurveData)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }

    curve.changed();
}

void UpdateCurve::update(ObjectPath&& model, QVector<QByteArray> &&segments)
{
    m_newCurveData = std::move(segments);
}

void UpdateCurve::serializeImpl(QDataStream& s) const
{
    s << m_model << m_oldCurveData << m_newCurveData;
}

void UpdateCurve::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_oldCurveData >> m_newCurveData;
}
