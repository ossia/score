// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/path/RelativePath.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <vector>

#include "RemoveProcessFromInterval.hpp"
#include <Process/ProcessList.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/ObjectPath.hpp>
#include <boost/range/adaptor/reversed.hpp>

// MOVEME
template<>
struct is_custom_serialized<std::vector<bool>> : std::true_type {};
template <>
struct TSerializer<DataStream, std::vector<bool>>
{
  static void
  readFrom(DataStream::Serializer& s, const std::vector<bool>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for (bool elt : vec)
      s.stream() << elt;

    ISCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<bool>& vec)
  {
    int32_t n;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for (int i = 0; i < n; i++)
    {
      bool b;
      s.stream() >> b;
      vec[i] = b;
    }

    ISCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

namespace Scenario
{
namespace Command
{

RemoveProcessFromInterval::RemoveProcessFromInterval(
    const IntervalModel& interval,
    Id<Process::ProcessModel>
        processId)
    : m_path{interval}, m_processId{std::move(processId)}
{
  // Save the process
  DataStream::Serializer s1{&m_serializedProcessData};
  auto& proc = interval.processes.at(m_processId);
  s1.readFrom(proc);

  m_smallView = interval.smallView();
  m_smallViewVisible = interval.smallViewVisible();
}

void RemoveProcessFromInterval::undo(const iscore::DocumentContext& ctx) const
{
  auto& interval = m_path.find(ctx);
  DataStream::Deserializer s{m_serializedProcessData};
  auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(fact, s, &interval);
  if (proc)
  {
    AddProcess(interval, proc);
  }
  else
  {
    ISCORE_TODO;
    return;
  }

  interval.replaceSmallView(m_smallView);
  interval.setSmallViewVisible(m_smallViewVisible);
}

void RemoveProcessFromInterval::redo(const iscore::DocumentContext& ctx) const
{
  auto& interval = m_path.find(ctx);
  // Find the slots that will be empty : we remove them.
  std::vector<int> slots_to_remove;
  int i = 0;
  for(const Slot& slot : interval.smallView())
  {
    if(slot.processes.size() == 1 && slot.processes[0] == m_processId)
      slots_to_remove.push_back(i);
    i++;
  }

  RemoveProcess(interval, m_processId);

  for(int slt : boost::adaptors::reverse(slots_to_remove))
    interval.removeSlot(slt);
}


void RemoveProcessFromInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processId << m_serializedProcessData << m_smallView << m_smallViewVisible;
}

void RemoveProcessFromInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId >> m_serializedProcessData >> m_smallView >> m_smallViewVisible;
}
}
}
