// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ShowRackInViewModel.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
ShowRack::ShowRack(const IntervalModel& vm) : m_intervalViewPath{vm}, m_old{vm.smallViewVisible()}
{
}

void ShowRack::undo(const score::DocumentContext& ctx) const
{
  auto& vm = m_intervalViewPath.find(ctx);
  vm.setSmallViewVisible(m_old);
}

void ShowRack::redo(const score::DocumentContext& ctx) const
{
  auto& vm = m_intervalViewPath.find(ctx);
  vm.setSmallViewVisible(true);
}

void ShowRack::serializeImpl(DataStreamInput& s) const
{
  s << m_intervalViewPath << m_old;
}

void ShowRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_intervalViewPath >> m_old;
}
}
}
