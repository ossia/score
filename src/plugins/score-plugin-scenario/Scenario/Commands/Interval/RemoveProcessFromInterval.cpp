// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoveProcessFromInterval.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <boost/range/adaptor/reversed.hpp>

namespace Scenario
{
namespace Command
{

RemoveProcessFromInterval::RemoveProcessFromInterval(
    const IntervalModel& interval,
    Id<Process::ProcessModel> processId)
    : m_path{interval}, m_processId{std::move(processId)}
{
  // Save the process
  DataStream::Serializer s1{&m_serializedProcessData};
  auto& proc = interval.processes.at(m_processId);
  s1.readFrom(proc);

  m_cables = Dataflow::saveCables({&proc}, score::IDocument::documentContext(interval));

  m_smallView = interval.smallView();
  m_smallViewVisible = interval.smallViewVisible();
}

void RemoveProcessFromInterval::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_path.find(ctx);
  DataStream::Deserializer s{m_serializedProcessData};
  auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(fact, s, ctx, &interval);
  SCORE_ASSERT(proc);
  AddProcess(interval, proc);

  Dataflow::restoreCables(m_cables, ctx);

  interval.replaceSmallView(m_smallView);
  interval.setSmallViewVisible(m_smallViewVisible);
}

void RemoveProcessFromInterval::redo(const score::DocumentContext& ctx) const
{
  Dataflow::removeCables(m_cables, ctx);

  auto& interval = m_path.find(ctx);
  // Find the slots that will be empty : we remove them.
  ossia::int_vector slots_to_remove;
  int i = 0;
  for (const Slot& slot : interval.smallView())
  {
    if (!slot.nodal && slot.processes.size() == 1 && slot.processes[0] == m_processId)
      slots_to_remove.push_back(i);
    i++;
  }

  RemoveProcess(interval, m_processId);

  for (int slt : boost::adaptors::reverse(slots_to_remove))
    interval.removeSlot(slt);
}

void RemoveProcessFromInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processId << m_serializedProcessData << m_cables << m_smallView
    << m_smallViewVisible;
}

void RemoveProcessFromInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId >> m_serializedProcessData >> m_cables >> m_smallView
      >> m_smallViewVisible;
}
}
}
