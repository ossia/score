#include "ScenarioSelection.hpp"

#include <Process/Dataflow/Port.hpp>

namespace Scenario
{

std::vector<QObject*>
findByAddress(const score::DocumentContext& ctx, const State::Address& root)
{
  std::vector<QObject*> matches;

  if(!root.isSet())
  {
    return matches;
  }
  else
  {
    matches.clear();
    std::vector<State::AddressAccessor> addresses;
    addresses.push_back(State::AddressAccessor{root});

    QList<QObject*> itemsToSearch;
    if(auto selection = ctx.selectionStack.currentSelection(); !selection.empty())
    {
      for(QObject* ptr : selection)
      {
        itemsToSearch.push_back(ptr);
        itemsToSearch.append(
            ptr->findChildren<QObject*>(QString{}, Qt::FindChildrenRecursively));
      }
    }
    else
    {
      itemsToSearch
          = ctx.document.findChildren<QObject*>(QString{}, Qt::FindChildrenRecursively);
    }
    ossia::remove_duplicates(itemsToSearch);
    if(itemsToSearch.empty())
      return matches;

    Selection sel{};

    {
      State::MessageList listCache;
      std::vector<QString> messagesCache;
      // Serialize ALL the things
      for(const auto& obj : itemsToSearch)
      {
        if(auto state = qobject_cast<const StateModel*>(obj))
        {
          auto& root = state->messages().rootNode();

          // First look for addresses containing the looked-up address
          bool must_add
              = Process::hasMatchingAddress(root, addresses, listCache, messagesCache);
          listCache.clear();
          messagesCache.clear();

          // If not found, then look for addresses containing the raw string
          //          if(!must_add)
          //            must_add = Process::hasMatchingText(root, stxt, listCache, messagesCache);
          // FIXME look into state processes?

          // Try to add if the searched text is in the name of the state
          if(must_add)
            matches.push_back(obj);
        }
        else if(auto event = qobject_cast<const EventModel*>(obj))
        {
          if(State::findAddressInExpression(event->condition(), root))
          {
            matches.push_back(obj);
            sel.append(*event);
          }
        }
        else if(auto ts = qobject_cast<const TimeSyncModel*>(obj))
        {
          if(State::findAddressInExpression(ts->expression(), root))
            matches.push_back(obj);
          sel.append(*ts);
        }
        else if(auto port = qobject_cast<Process::Inlet*>(obj))
        {
          if(State::addressIsChildOf(root, port->address().address))
          {
            matches.push_back(port);
          }
        }
        else if(auto port = qobject_cast<Process::Outlet*>(obj))
        {
          if(State::addressIsChildOf(root, port->address().address))
          {
            matches.push_back(port);
          }
        }
        /*
        else if(auto cmt = qobject_cast<const CommentBlockModel*>(obj))
        {
        }
        else if(auto interval = qobject_cast<const IntervalModel*>(obj))
        {
        }
        */
      }
    }
    ossia::remove_duplicates(matches);
    // score::SelectionDispatcher d{doc->context().selectionStack};
    // d.select(sel);
  }

  return matches;
}

}
