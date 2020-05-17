// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RecordAutomationCreationVisitor.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <ossia/network/domain/domain.hpp>

namespace Recording
{

RecordData RecordAutomationCreationVisitor::makeCurve(float start_y)
{
  // Note : since we directly create the IDs here, we don't have to worry
  // about their generation.
  auto cmd_proc = new Scenario::Command::AddOnlyProcessToInterval{
      box.interval, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}, {}};
  cmd_proc->redo(recorder.context.context);
  auto& proc = box.interval.processes.at(cmd_proc->processId());
  auto& autom = static_cast<Automation::ProcessModel&>(proc);

  auto cmd_layer
      = new Scenario::Command::AddLayerModelToSlot{Scenario::SlotPath{box.interval, 0}, proc};
  cmd_layer->redo(recorder.context.context);

  autom.curve().clear();

  // TODO handle other domain types for vec.
  auto& dom = addr.domain.get();
  auto min = dom.convert_min<float>();
  auto max = dom.convert_max<float>();

  Curve::SegmentData seg;
  seg.id = Id<Curve::SegmentModel>{0};
  seg.start = {0, start_y};
  seg.end = {1, -1};
  seg.specificSegmentData
      = QVariant::fromValue(Curve::PointArraySegmentData{0, 1, min, max, {{0, start_y}}});
  auto segt = new Curve::PointArraySegment{seg, &autom.curve()};

  segt->setStart({0, start_y});
  segt->setEnd({1, -1});
  segt->addPoint(0, start_y);

  autom.curve().addSegment(segt);
  return RecordData{cmd_proc, cmd_layer, autom.curve(), *segt, addr.unit};
}

void RecordAutomationCreationVisitor::handle_numeric(float val)
{
  addresses.back().push_back(Device::address(node).address);
  recorder.numeric_records.insert(std::make_pair(addresses.back().back(), makeCurve(val)));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 2> val)
{
  // here we create one curve per component.

  // The address is added only once
  addresses.back().push_back(Device::address(node).address);
  recorder.vec2_records.insert(std::make_pair(
      addresses.back().back(), std::array<RecordData, 2>{makeCurve(val[0]), makeCurve(val[1])}));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 3> val)
{
  // here we create one curve per component.

  // The address is added only once
  addresses.back().push_back(Device::address(node).address);
  recorder.vec3_records.insert(std::make_pair(
      addresses.back().back(),
      std::array<RecordData, 3>{makeCurve(val[0]), makeCurve(val[1]), makeCurve(val[2])}));
}

void RecordAutomationCreationVisitor::operator()(std::array<float, 4> val)
{
  // here we create one curve per component.

  // The address is added only once
  addresses.back().push_back(Device::address(node).address);
  recorder.vec4_records.insert(std::make_pair(
      addresses.back().back(),
      std::array<RecordData, 4>{
          makeCurve(val[0]), makeCurve(val[1]), makeCurve(val[2]), makeCurve(val[3])}));
}

void RecordAutomationCreationVisitor::operator()(float f)
{
  handle_numeric(f);
}

void RecordAutomationCreationVisitor::operator()(int f)
{
  handle_numeric(f);
}

void RecordAutomationCreationVisitor::operator()(char f)
{
  handle_numeric(f);
}

void RecordAutomationCreationVisitor::operator()(bool f)
{
  handle_numeric(f);
}
}
