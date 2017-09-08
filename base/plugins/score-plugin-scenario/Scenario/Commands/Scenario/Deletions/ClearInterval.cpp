// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include "ClearInterval.hpp"
#include <Process/Process.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
ClearInterval::ClearInterval(const IntervalModel& interval)
    : m_intervalSaveData{interval}
{
}

void ClearInterval::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_intervalSaveData.intervalPath.find(ctx);

  m_intervalSaveData.reload(interval);
}

void ClearInterval::redo(const score::DocumentContext& ctx) const
{
  auto& interval = m_intervalSaveData.intervalPath.find(ctx);

  interval.clearSmallView();

  // We make copies since the iterators might change.
  // TODO check if this is still valid wrt boost::multi_index
  auto processes = shallow_copy(interval.processes);
  for (auto process : processes)
  {
    RemoveProcess(interval, process->id());
  }
}

void ClearInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_intervalSaveData;
}

void ClearInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_intervalSaveData;
}
}
}
