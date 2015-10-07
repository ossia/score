#include "UpdateCurve.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"

UpdateCurve::UpdateCurve(
        Path<CurveModel>&& model,
        std::vector<CurveSegmentData>&& segments):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_model{std::move(model)},
    m_newCurveData{std::move(segments)}
{
    const auto& curve = m_model.find();
    m_oldCurveData = curve.toCurveData();
}

void UpdateCurve::undo()
{
    auto& curve = m_model.find();
    curve.fromCurveData(m_oldCurveData);
}

void UpdateCurve::redo()
{
    auto& curve = m_model.find();
    curve.fromCurveData(m_newCurveData);
}

void UpdateCurve::update(
        const Path<CurveModel>& model,
        std::vector<CurveSegmentData>&& segments)
{
    m_newCurveData = std::move(segments);
}



void UpdateCurve::serializeImpl(QDataStream& s) const
{
    // TODO serialize std::vector (there are already existing methods for this)
    s << m_model
      << QVector<CurveSegmentData>::fromStdVector(m_oldCurveData)
      << QVector<CurveSegmentData>::fromStdVector(m_newCurveData);
}

void UpdateCurve::deserializeImpl(QDataStream& s)
{
    QVector<CurveSegmentData> old_one;
    QVector<CurveSegmentData> new_one;
    s >> m_model >> old_one >> new_one;

    m_oldCurveData = old_one.toStdVector();
    m_newCurveData = new_one.toStdVector();
}
