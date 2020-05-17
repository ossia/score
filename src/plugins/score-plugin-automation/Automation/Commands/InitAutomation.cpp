// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InitAutomation.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Automation
{
InitAutomation::InitAutomation(
    const ProcessModel& path,
    const ::State::AddressAccessor& newaddr,
    double newmin,
    double newmax,
    std::vector<Curve::SegmentData>&& segments)
    : m_path{path}
    , m_addr(newaddr)
    , m_newMin{newmin}
    , m_newMax{newmax}
    , m_segments{std::move(segments)}
{
}

InitAutomation::InitAutomation(
    const ProcessModel& path,
    State::AddressAccessor&& newaddr,
    double newmin,
    double newmax,
    std::vector<Curve::SegmentData>&& segments)
    : m_path{path}
    , m_addr(std::move(newaddr))
    , m_newMin{newmin}
    , m_newMax{newmax}
    , m_segments{std::move(segments)}
{
}

InitAutomation::InitAutomation(
    const ProcessModel& path,
    const ::State::AddressAccessor& newaddr,
    double newmin,
    double newmax)
    : InitAutomation(path, newaddr, newmin, newmax, {})
{
}
void InitAutomation::undo(const score::DocumentContext& ctx) const { }

void InitAutomation::redo(const score::DocumentContext& ctx) const
{
  auto& autom = m_path.find(ctx);

  auto& curve = autom.curve();

  if (!m_segments.empty())
    curve.fromCurveData(m_segments);

  autom.setMin(m_newMin);
  autom.setMax(m_newMax);

  autom.setAddress(m_addr);
}

void InitAutomation::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_addr << m_newMin << m_newMax << m_segments;
}

void InitAutomation::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_addr >> m_newMin >> m_newMax >> m_segments;
}
}
