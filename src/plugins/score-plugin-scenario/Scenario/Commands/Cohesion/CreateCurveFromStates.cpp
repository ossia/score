// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateCurveFromStates.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <State/ValueSerialization.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>

#include <QByteArray>

#include <Color/GradientModel.hpp>

namespace Scenario
{
namespace Command
{
CreateAutomationFromStates::CreateAutomationFromStates(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    const Curve::CurveDomain& dom,
    bool tween)
    : CreateProcessAndLayers{interval, slotList, std::move(curveId), Metadata<ConcreteKey_k, Automation::ProcessModel>::get()}
    , m_address{std::move(address)}
    , m_dom{dom}
    , m_tween(tween)
{
}

void CreateAutomationFromStates::redo(const score::DocumentContext& ctx) const
{
  m_addProcessCmd.redo(ctx);
  auto& cstr = m_addProcessCmd.intervalPath().find(ctx);
  auto& autom
      = safe_cast<Automation::ProcessModel&>(cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.curve().clear();
  autom.setTween(m_tween);

  // Add a segment
  auto segment = new Curve::DefaultCurveSegmentModel{
      Id<Curve::SegmentModel>{score::id_generator::getFirstId()}, &autom.curve()};

  double fact = 1. / (m_dom.max - m_dom.min);
  segment->setStart({0., (m_dom.start - m_dom.min) * fact});
  segment->setEnd({1., (m_dom.end - m_dom.min) * fact});

  autom.setMin(m_dom.min);
  autom.setMax(m_dom.max);

  autom.curve().addSegment(segment);

  autom.curve().changed();

  for (const auto& cmd : m_slotsCmd)
    cmd.redo(ctx);
}

void CreateAutomationFromStates::serializeImpl(DataStreamInput& s) const
{
  CreateProcessAndLayers::serializeImpl(s);
  s << m_address << m_dom << m_tween;
}

void CreateAutomationFromStates::deserializeImpl(DataStreamOutput& s)
{
  CreateProcessAndLayers::deserializeImpl(s);
  s >> m_address >> m_dom >> m_tween;
}

CreateGradient::CreateGradient(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    QColor start,
    QColor end,
    bool tween)
    : CreateProcessAndLayers{interval, slotList, std::move(curveId), Metadata<ConcreteKey_k, Gradient::ProcessModel>::get()}
    , m_address{std::move(address)}
    , m_start{start}
    , m_end{end}
    , m_tween(tween)
{
}

void CreateGradient::redo(const score::DocumentContext& ctx) const
{
  m_addProcessCmd.redo(ctx);
  auto& cstr = m_addProcessCmd.intervalPath().find(ctx);
  auto& autom = safe_cast<Gradient::ProcessModel&>(cstr.processes.at(m_addProcessCmd.processId()));
  autom.outlet->setAddress(m_address);
  autom.setTween(m_tween);

  Gradient::ProcessModel::gradient_colors g;
  g[0.] = m_start;
  g[1.] = m_end;
  autom.setGradient(std::move(g));

  for (const auto& cmd : m_slotsCmd)
    cmd.redo(ctx);
}

void CreateGradient::serializeImpl(DataStreamInput& s) const
{
  CreateProcessAndLayers::serializeImpl(s);
  s << m_address << m_tween;
}

void CreateGradient::deserializeImpl(DataStreamOutput& s)
{
  CreateProcessAndLayers::deserializeImpl(s);
  s >> m_address >> m_tween;
}

CreateInterpolationFromStates::CreateInterpolationFromStates(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    ossia::value start,
    ossia::value end,
    bool tween)
    : CreateProcessAndLayers{interval, slotList, std::move(curveId), Metadata<ConcreteKey_k, Automation::ProcessModel>::get()}
    , m_address{std::move(address)}
    , m_start{std::move(start)}
    , m_end{std::move(end)}
    , m_tween{tween}
{
}

void CreateInterpolationFromStates::redo(const score::DocumentContext& ctx) const
{
  m_addProcessCmd.redo(ctx);

  auto& cstr = m_addProcessCmd.intervalPath().find(ctx);
  auto& autom
      = safe_cast<Interpolation::ProcessModel&>(cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.setStart(m_start);
  autom.setEnd(m_end);

  for (const auto& cmd : m_slotsCmd)
    cmd.redo(ctx);
}

void CreateInterpolationFromStates::serializeImpl(DataStreamInput& s) const
{
  CreateProcessAndLayers::serializeImpl(s);
  s << m_address << m_start << m_end << m_tween;
}

void CreateInterpolationFromStates::deserializeImpl(DataStreamOutput& s)
{
  CreateProcessAndLayers::deserializeImpl(s);
  s >> m_address >> m_start >> m_end >> m_tween;
}

CreateProcessAndLayers::CreateProcessAndLayers(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> procId,
    UuidKey<Process::ProcessModel> key)
    : m_addProcessCmd{std::move(interval), std::move(procId), std::move(key), QString{}, QPointF{}}
{
  m_slotsCmd.reserve(slotList.size());
  for (const auto& elt : slotList)
  {
    m_slotsCmd.emplace_back(elt, procId);
  }
}

void CreateProcessAndLayers::undo(const score::DocumentContext& ctx) const
{
  for (const auto& cmd : m_slotsCmd)
    cmd.undo(ctx);
  m_addProcessCmd.undo(ctx);
}

void CreateProcessAndLayers::serializeImpl(DataStreamInput& s) const
{
  s << m_addProcessCmd.serialize();
  s << (int32_t)m_slotsCmd.size();
  for (const auto& elt : m_slotsCmd)
  {
    s << elt.serialize();
  }
}

void CreateProcessAndLayers::deserializeImpl(DataStreamOutput& s)
{
  QByteArray a;
  s >> a;
  m_addProcessCmd.deserialize(a);

  int32_t n = 0;
  s >> n;
  m_slotsCmd.resize(n);
  for (int i = 0; i < n; i++)
  {
    QByteArray b;
    s >> b;
    m_slotsCmd.at(i).deserialize(b);
  }
}
}
}
