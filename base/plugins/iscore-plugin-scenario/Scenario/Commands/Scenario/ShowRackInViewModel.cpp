// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <algorithm>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "ShowRackInViewModel.hpp"
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
ShowRack::ShowRack(
    const IntervalModel& vm)
    : m_intervalViewPath{vm}
{
}


void ShowRack::undo(const iscore::DocumentContext& ctx) const
{
  auto& vm = m_intervalViewPath.find(ctx);
  vm.setSmallViewVisible(false);
}

void ShowRack::redo(const iscore::DocumentContext& ctx) const
{
  auto& vm = m_intervalViewPath.find(ctx);
  vm.setSmallViewVisible(true);
}

void ShowRack::serializeImpl(DataStreamInput& s) const
{
  s << m_intervalViewPath;
}

void ShowRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_intervalViewPath;
}
}
}
