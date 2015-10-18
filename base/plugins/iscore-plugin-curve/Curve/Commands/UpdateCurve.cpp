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

void UpdateCurve::undo() const
{
    auto& curve = m_model.find();
    curve.fromCurveData(m_oldCurveData);
}

void UpdateCurve::redo() const
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
    s << m_model
      << m_oldCurveData
      << m_newCurveData;
}

void UpdateCurve::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_oldCurveData >> m_newCurveData;
}
