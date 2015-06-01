#include "UpdateCurve.hpp"
#include "CurveModel.hpp"
#include "CurveSegmentModelSerialization.hpp"

UpdateCurve::UpdateCurve(ObjectPath&& model, QVector<CurveSegmentModel*> segments):
    iscore::SerializableCommand("CurveControl", className(), description()),
    m_model(std::move(model))
{
    const auto& curve = m_model.find<CurveModel>();
    for(const auto& segment : curve.segments())
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        m_oldCurveData.append(arr);
    }

    for(const auto& segment : segments)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        m_newCurveData.append(arr);
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
}

void UpdateCurve::update(ObjectPath&& model, QVector<CurveSegmentModel*> segments)
{
    m_newCurveData.clear();

    for(const auto& segment : segments)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        m_newCurveData.append(arr);
    }
}

void UpdateCurve::serializeImpl(QDataStream& s) const
{
    s << m_model << m_oldCurveData << m_newCurveData;
}

void UpdateCurve::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_oldCurveData >> m_newCurveData;
}
