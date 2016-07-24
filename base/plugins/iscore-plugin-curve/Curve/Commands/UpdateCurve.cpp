#include <algorithm>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "UpdateCurve.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Curve
{
UpdateCurve::UpdateCurve(
        Path<Model>&& model,
        std::vector<SegmentData>&& segments):
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
        const Path<Model>& model,
        std::vector<SegmentData>&& segments)
{
    m_newCurveData = std::move(segments);
}



void UpdateCurve::serializeImpl(DataStreamInput& s) const
{
    s << m_model
      << m_oldCurveData
      << m_newCurveData;
}

void UpdateCurve::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_oldCurveData >> m_newCurveData;
}
}
