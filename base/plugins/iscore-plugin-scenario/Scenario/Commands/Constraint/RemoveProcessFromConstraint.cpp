#include <Process/Process.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <iscore/model/path/RelativePath.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <vector>

#include "RemoveProcessFromConstraint.hpp"
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

RemoveProcessFromConstraint::RemoveProcessFromConstraint(
    const ConstraintModel& constraint,
    Id<Process::ProcessModel>
        processId)
    : m_path{constraint}, m_processId{std::move(processId)}
{
  // Save the process
  DataStream::Serializer s1{&m_serializedProcessData};
  auto& proc = constraint.processes.at(m_processId);
  s1.readFrom(proc);

  m_smallView = constraint.smallView();
  m_smallViewVisible = constraint.smallViewVisible();
}

void RemoveProcessFromConstraint::undo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  DataStream::Deserializer s{m_serializedProcessData};
  auto& fact = context.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(fact, s, &constraint);
  if (proc)
  {
    AddProcess(constraint, proc);
  }
  else
  {
    ISCORE_TODO;
    return;
  }

  constraint.replaceSmallView(m_smallView);
  constraint.setSmallViewVisible(m_smallViewVisible);
}

void RemoveProcessFromConstraint::redo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  // Find the slots that will be empty : we remove them.
  std::vector<int> slots_to_remove;
  int i = 0;
  for(const Slot& slot : constraint.smallView())
  {
    if(slot.processes.size() == 1 && slot.processes[0] == m_processId)
      slots_to_remove.push_back(i);
    i++;
  }

  RemoveProcess(constraint, m_processId);

  for(int slt : boost::adaptors::reverse(slots_to_remove))
    constraint.removeSlot(slt);
}


void RemoveProcessFromConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processId << m_serializedProcessData << m_smallView << m_smallViewVisible;
}

void RemoveProcessFromConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId >> m_serializedProcessData >> m_smallView >> m_smallViewVisible;
}
}
}
