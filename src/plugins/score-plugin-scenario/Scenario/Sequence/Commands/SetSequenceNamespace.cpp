// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetSequenceNamespace.hpp"

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
