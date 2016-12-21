#include <Interpolation/Commands/ChangeAddress.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Interpolation
{
ChangeAddress::ChangeAddress(
    const ProcessModel& proc,
    const State::AddressAccessor& addr,
    const State::Value& start,
    const State::Value& end,
    const State::Unit& u)
    : m_path{proc}
    , m_oldAddr{proc.address()}
    , m_newAddr{addr}
    , m_oldUnit{proc.sourceUnit()}
    , m_newUnit{u}
    , m_oldStart{proc.start()}
    , m_newStart{start}
    , m_oldEnd{proc.end()}
    , m_newEnd{end}
{
}

void ChangeAddress::undo() const
{
  auto& interp = m_path.find();

  interp.setStart(m_oldStart);
  interp.setEnd(m_oldEnd);
  interp.setSourceUnit(m_oldUnit);
  interp.setAddress(m_oldAddr);

  interp.curve().changed();
}

void ChangeAddress::redo() const
{
  auto& interp = m_path.find();

  interp.setStart(m_newStart);
  interp.setEnd(m_newEnd);
  interp.setSourceUnit(m_newUnit);
  interp.setAddress(m_newAddr);

  interp.curve().changed();
}

void ChangeAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_oldAddr << m_newAddr << m_oldUnit << m_newUnit << m_oldStart
    << m_newStart << m_oldEnd << m_newEnd;
}

void ChangeAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_oldAddr >> m_newAddr >> m_oldUnit >> m_newUnit >> m_oldStart
      >> m_newStart >> m_oldEnd >> m_newEnd;
}
}
