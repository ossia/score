// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "SetSegmentParameters.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>

namespace Curve
{
SetSegmentParameters::SetSegmentParameters(
    const Model& curve,
    SegmentParameterMap&& parameters)
    : m_model{curve}
    , m_new{std::move(parameters)}
{
  for (auto it = m_new.cbegin(); it != m_new.cend(); ++it)
  {
    const auto& seg = curve.segments().at(it->first);
    m_old.insert(
        {it->first, {seg.verticalParameter(), seg.horizontalParameter()}});
  }
}

void SetSegmentParameters::undo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  for (auto it = m_old.cbegin(); it != m_old.cend(); ++it)
  {
    auto& seg = curve.segments().at(it->first);

    if (it->second.first)
      seg.setVerticalParameter(*it->second.first);
    if (it->second.second)
      seg.setHorizontalParameter(*it->second.second);
  }

  curve.changed();
}

void SetSegmentParameters::redo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  for (auto it = m_new.cbegin(); it != m_new.cend(); ++it)
  {
    auto& seg = curve.segments().at(it->first);

    seg.setVerticalParameter(it->second.first);
    seg.setHorizontalParameter(it->second.second);
  }

  curve.changed();
}

void SetSegmentParameters::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void SetSegmentParameters::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
