// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <State/ValueSerialization.hpp>

namespace Interpolation
{
ChangeAddress::ChangeAddress(
    const ProcessModel& proc,
    const State::AddressAccessor& addr,
    const ossia::value& start,
    const ossia::value& end,
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

void ChangeAddress::undo(const score::DocumentContext& ctx) const
{
  auto& interp = m_path.find(ctx);

  interp.setStart(m_oldStart);
  interp.setEnd(m_oldEnd);
  interp.setSourceUnit(m_oldUnit);
  interp.setAddress(m_oldAddr);

  interp.curve().changed();
}

void ChangeAddress::redo(const score::DocumentContext& ctx) const
{
  auto& interp = m_path.find(ctx);

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

void ChangeInterpolationAddress(const ProcessModel& proc, const State::AddressAccessor& addr, CommandDispatcher<>& disp)
{
  // Various checks
  if (addr == proc.address())
    return;

  if (addr.address.path.isEmpty())
    return;

  if (addr == State::AddressAccessor{})
  {
    disp.submitCommand(new ChangeAddress{proc, {}, {}, {}, {}});
  }
  else
  {
    // Try to find a matching state in the start & end state in order to update
    // the process
    auto cst = dynamic_cast<Scenario::IntervalModel*>(proc.parent());
    if (!cst)
      return;
    auto parent_scenario
        = dynamic_cast<Scenario::ScenarioInterface*>(cst->parent());
    if (!parent_scenario)
      return;

    ossia::value sv, ev;
    ossia::unit_t source_u;

    auto& ss = Scenario::startState(*cst, *parent_scenario);
    auto& es = Scenario::endState(*cst, *parent_scenario);
    const auto snodes
        = Process::try_getNodesFromAddress(ss.messages().rootNode(), addr);
    const auto enodes
        = Process::try_getNodesFromAddress(es.messages().rootNode(), addr);

    for (const Process::MessageNode* lhs : snodes)
    {
      if (!lhs->hasValue())
        continue;
      if (lhs->name.qualifiers.get().accessors
          != addr.qualifiers.get().accessors)
        continue;

      auto it = ossia::find_if(enodes, [&](auto rhs) {
        return (lhs->name.qualifiers == rhs->name.qualifiers)
            && rhs->hasValue();
      });

      if (it != enodes.end())
      {
        sv = *lhs->value();
        ev = *(*it)->value();
        source_u = lhs->name.qualifiers.get().unit;

        break; // or maybe not break ? the latest should replace maybe ?
      }
    }

    disp.submitCommand(
          new ChangeAddress{proc, addr, sv, ev, source_u});
  }

}

}
