#include "RebindReference.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/State/MessageNode.hpp>

#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <LocalTree/ScriptableReference.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario::Command
{
void rebindReference(
    QObject& referrer, const ::State::Address& from, const ::State::Address& to,
    const score::DocumentContext& ctx)
{
  RedoMacroCommandDispatcher<RebindReferenceMacro> disp{ctx.commandStack};
  auto mapping = [&](const ::State::AddressAccessor& acc)
      -> std::optional<::State::AddressAccessor> {
    if(acc.address != from)
      return std::nullopt;
    auto next = acc;
    next.address = to;
    return next;
  };

  if(auto state = qobject_cast<StateModel*>(&referrer))
  {
    auto tree = state->messages().rootNode();
    ::State::MessageList moved;
    std::vector<::State::AddressAccessor> removed;
    auto visit = [&](auto& self, const Process::MessageNode& n) -> void {
      if(n.values.userValue)
      {
        auto acc = Process::address(n);
        if(auto next = mapping(acc))
        {
          removed.push_back(acc);
          moved.push_back({*next, *n.values.userValue});
        }
      }
      for(auto& child : n)
        self(self, child);
    };
    visit(visit, tree);
    if(!moved.empty())
    {
      for(auto& acc : removed)
        updateTreeWithRemovedUserMessage(tree, acc);
      disp.submit(new ReplaceState{*state, state->messages().rootNode(), tree, moved});
    }
  }
  else if(auto port = qobject_cast<Process::Port*>(&referrer))
  {
    if(auto next = mapping(port->address()))
      disp.submit(new Process::ChangePortAddress{*port, *next});
  }
  else if(auto ev = qobject_cast<EventModel*>(&referrer))
  {
    auto e = ev->condition();
    bool changed = false;
    LocalTree::mapAddresses(e, mapping, changed);
    if(changed)
      disp.submit(new SetCondition{*ev, std::move(e)});
  }
  else if(auto ts = qobject_cast<TimeSyncModel*>(&referrer))
  {
    auto e = ts->expression();
    bool changed = false;
    LocalTree::mapAddresses(e, mapping, changed);
    if(changed)
      disp.submit(new SetTrigger{*ts, std::move(e)});
  }
  disp.commit();
}
}
