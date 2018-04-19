// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RecordAutomationFirstParameterCallbackVisitor.hpp"

#include <Automation/AutomationModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

namespace Recording
{

void RecordAutomationFirstCallbackVisitor::operator()(std::array<float, 2> val)
{
  auto it = recorder.vec2_records.find(addr);
  SCORE_ASSERT(it != recorder.vec2_records.end());

  const auto& proc_data = it->second;

  const constexpr std::size_t N = 2;
  for (std::size_t i = 0; i < N; i++)
    proc_data[i].segment.addPoint(0, val[i]);
}

void RecordAutomationFirstCallbackVisitor::operator()(std::array<float, 3> val)
{
  auto it = recorder.vec3_records.find(addr);
  SCORE_ASSERT(it != recorder.vec3_records.end());

  const auto& proc_data = it->second;

  const constexpr std::size_t N = 3;
  for (std::size_t i = 0; i < N; i++)
    proc_data[i].segment.addPoint(0, val[i]);
}

void RecordAutomationFirstCallbackVisitor::operator()(std::array<float, 4> val)
{
  auto it = recorder.vec4_records.find(addr);
  SCORE_ASSERT(it != recorder.vec4_records.end());

  const auto& proc_data = it->second;

  const constexpr std::size_t N = 4;
  for (std::size_t i = 0; i < N; i++)
    proc_data[i].segment.addPoint(0, val[i]);
}

void RecordAutomationFirstCallbackVisitor::handle_numeric(float newval)
{
  auto it = recorder.numeric_records.find(addr);
  SCORE_ASSERT(it != recorder.numeric_records.end());

  const auto& proc_data = it->second;
  proc_data.segment.addPoint(0, newval);
}

void RecordAutomationFirstCallbackVisitor::operator()(float f)
{
  handle_numeric(f);
}

void RecordAutomationFirstCallbackVisitor::operator()(int f)
{
  handle_numeric(f);
}

void RecordAutomationFirstCallbackVisitor::operator()(char f)
{
  handle_numeric(f);
}

void RecordAutomationFirstCallbackVisitor::operator()(bool f)
{
  handle_numeric(f);
}
}
