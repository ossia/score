// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HideRackInViewModel.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
HideRack::HideRack(const Scenario::IntervalModel& interval_vm) : m_path{interval_vm} { }

void HideRack::undo(const score::DocumentContext& ctx) const
{
  auto& vm = m_path.find(ctx);
  vm.setSmallViewVisible(true);
}

void HideRack::redo(const score::DocumentContext& ctx) const
{
  auto& vm = m_path.find(ctx);
  vm.setSmallViewVisible(false);
}

void HideRack::serializeImpl(DataStreamInput& s) const
{
  s << m_path;
}

void HideRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path;
}
}
}
