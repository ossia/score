// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <algorithm>

#include "UpdateCurve.hpp"
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Curve
{
UpdateCurve::UpdateCurve(
    const Model& model, std::vector<SegmentData>&& segments)
    : m_model{std::move(model)}
    , m_oldCurveData{model.toCurveData()}
    , m_newCurveData{std::move(segments)}
{
}

void UpdateCurve::undo(const iscore::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  curve.fromCurveData(m_oldCurveData);
}

void UpdateCurve::redo(const iscore::DocumentContext& ctx) const
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
