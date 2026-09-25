// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetCondition.hpp"

#include <State/Expression.hpp>

#include <Scenario/Document/Event/EventModel.hpp>

#include <LocalTree/ScriptableReference.hpp>
#include <LocalTree/ScriptableScenarioComponent.hpp>

#include <score/document/DocumentInterface.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
SetCondition::SetCondition(const EventModel& event, State::Expression&& cond)
    : m_path{event}
    , m_condition(std::move(cond))
    , m_previousCondition{event.condition()}
{
  LocalTree::settle(m_condition, score::IDocument::documentContext(event));
}

void SetCondition::undo(const score::DocumentContext& ctx) const
{
  auto& event = m_path.find(ctx);
  event.setCondition(m_previousCondition);
}

void SetCondition::redo(const score::DocumentContext& ctx) const
{
  auto& event = m_path.find(ctx);
  event.setCondition(m_condition);
}

void SetCondition::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_condition >> m_previousCondition;
}

static State::Expression publishedCondition(const EventModel& event)
{
  const auto addr = LocalTree::scriptableAddress(event).toString();
  if(addr.isEmpty())
    return {};
  if(auto e = State::parseExpression(QStringLiteral("%%1% == true").arg(addr)))
    return *e;
  return {};
}

void setEventScriptable(
    const EventModel& event, bool scriptable, const score::DocumentContext& ctx)
{
  if(scriptable == event.scriptable())
    return;

  RedoMacroCommandDispatcher<SetEventScriptableMacro> disp{ctx.commandStack};
  if(scriptable)
  {
    disp.submit(new SetEventScriptable{event, true});
    if(!event.active())
      if(auto e = publishedCondition(event); e.hasChildren())
        disp.submit(new SetCondition{event, std::move(e)});
  }
  else
  {
    if(event.condition() == publishedCondition(event))
      disp.submit(new SetCondition{event, State::Expression{}});
    disp.submit(new SetEventScriptable{event, false});
  }
  disp.commit();
}

SetOffsetBehavior::SetOffsetBehavior(const EventModel& event, OffsetBehavior newval)
    : score::PropertyCommand{event, "offsetBehavior", QVariant::fromValue(newval)}
{
}
}
}
