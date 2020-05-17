// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UpdateCurve.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Curve
{
UpdateCurve::UpdateCurve(const Model& model, std::vector<SegmentData>&& segments)
    : m_model{std::move(model)}
    , m_oldCurveData{model.toCurveData()}
    , m_newCurveData{std::move(segments)}
{
}

void UpdateCurve::undo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  curve.fromCurveData(m_oldCurveData);
}

void UpdateCurve::redo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  curve.fromCurveData(m_newCurveData);
}

void UpdateCurve::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_oldCurveData << m_newCurveData;
}

void UpdateCurve::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_oldCurveData >> m_newCurveData;
}
}
