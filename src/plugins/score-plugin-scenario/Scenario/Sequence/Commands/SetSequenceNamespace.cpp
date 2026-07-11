// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetSequenceNamespace.hpp"

#include <Process/State/MessageNode.hpp>

#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <State/ValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Sequence
{
namespace Command
{

AddSequenceParameter::AddSequenceParameter(
    const SequenceModel& seq, State::AddressAccessor addr)
    : m_path{seq}
    , m_addr{std::move(addr)}
{
}

void AddSequenceParameter::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeParameter(m_addr);
}

void AddSequenceParameter::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).addParameter(m_addr, {});
}

void AddSequenceParameter::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_addr;
}

void AddSequenceParameter::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_addr;
}

SetSequenceISValue::SetSequenceISValue(
    const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId,
    State::AddressAccessor addr, ossia::value val)
    : m_path{seq}
    , m_tsId{tsId}
    , m_addr{std::move(addr)}
    , m_newVal{std::move(val)}
{
  // Capture the previous value (if any) from the IS state for undo.
  const auto stId = seq.stateForTimeSync(tsId);
  const auto& st = seq.states.at(stId);
  for(const auto& msg : Process::flatten(st.messages().rootNode()))
  {
    if(msg.address == m_addr)
    {
      m_oldVal = msg.value;
      m_hadOldVal = true;
      break;
    }
  }
}

void SetSequenceISValue::undo(const score::DocumentContext& ctx) const
{
  if(m_hadOldVal)
    m_path.find(ctx).setISValue(m_tsId, m_addr, m_oldVal);
  // If there was no previous value we leave the message in place: the whole
  // gesture-level undo removes the surrounding structure anyway.
}

void SetSequenceISValue::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setISValue(m_tsId, m_addr, m_newVal);
}

void SetSequenceISValue::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_tsId << m_addr << m_newVal << m_oldVal << m_hadOldVal;
}

void SetSequenceISValue::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_tsId >> m_addr >> m_newVal >> m_oldVal >> m_hadOldVal;
}

RemoveSequenceParameter::RemoveSequenceParameter(
    const SequenceModel& seq, State::AddressAccessor addr)
    : m_path{seq}
    , m_addr{std::move(addr)}
{
}

void RemoveSequenceParameter::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).addParameter(m_addr, {});
}

void RemoveSequenceParameter::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeParameter(m_addr);
}

void RemoveSequenceParameter::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_addr;
}

void RemoveSequenceParameter::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_addr;
}

}
}
