#include "UpdateCurve.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"

UpdateCurve::UpdateCurve(
        Path<CurveModel>&& model,
        QVector<QByteArray> &&segments):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_model{std::move(model)},
    m_newCurveData{std::move(segments)}
{
    const auto& curve = m_model.find();
    for(const auto& segment : curve.segments())
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(segment);
        m_oldCurveData.append(arr);
    }
}

void UpdateCurve::undo()
{
    auto& curve = m_model.find();
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
    auto& curve = m_model.find();
    curve.clear();

    for(const auto& elt : m_newCurveData)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }

    curve.changed();
}

void UpdateCurve::update(
        Path<CurveModel>&& model,
        QVector<QByteArray> &&segments)
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


///////////////////
///////////////////
///////////////////
///////////////////


UpdateCurveFast::UpdateCurveFast(
        Path<CurveModel>&& model,
        const QVector<CurveSegmentData> &segments):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_model{std::move(model)},
    m_newCurveData{segments}
{
    const auto& curve = m_model.find();
    m_oldCurveData = curve.toCurveData();
}

void UpdateCurveFast::undo()
{
    auto& curve = m_model.find();
    curve.fromCurveData(m_oldCurveData);
}

void UpdateCurveFast::redo()
{
    auto& curve = m_model.find();
    curve.fromCurveData(m_newCurveData);
}

void UpdateCurveFast::update(
        const Path<CurveModel>& model,
        const QVector<CurveSegmentData>& segments)
{
    m_newCurveData = std::move(segments);
}

void UpdateCurveFast::serializeImpl(QDataStream& s) const
{
    s << m_model << m_oldCurveData << m_newCurveData;
}

void UpdateCurveFast::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_oldCurveData >> m_newCurveData;
}
