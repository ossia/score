#include "PasteAnchors.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/State/MessageNode.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <LocalTree/ScriptableReference.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/std/HashMap.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash.hpp>

#include <algorithm>

namespace Scenario
{
bool copiedFromHere(const rapidjson::Value& copy, const score::DocumentContext& ctx)
{
  if(!copy.IsObject())
    return false;
  auto it = copy.FindMember("OriginDocument");
  return it != copy.MemberEnd() && it->value.IsString()
         && QString::fromUtf8(it->value.GetString()) == score::IDocument::copyOrigin(ctx.document);
}

bool hasAnchors(const rapidjson::Value& copy)
{
  if(copy.IsObject())
  {
    for(auto& m : copy.GetObject())
      if(m.name == "Target" || hasAnchors(m.value))
        return true;
  }
  else if(copy.IsArray())
  {
    for(auto& v : copy.GetArray())
      if(hasAnchors(v))
        return true;
  }
  return false;
}

namespace
{
template <typename T, typename F>
void forEach(QObject& root, F&& f)
{
  if(auto self = qobject_cast<T*>(&root))
    f(*self);
  for(auto child : root.findChildren<T*>())
    f(*child);
}
}

namespace
{
//! Hashes of the successive prefixes of a path: [0], [0, 1], ...
template <typename F>
void forEachPrefixHash(const ObjectIdentifierVector& path, F&& f)
{
  std::size_t seed = 0;
  for(std::size_t i = 0; i < path.size(); i++)
  {
    ossia::hash_combine(seed, std::hash<ObjectIdentifier>{}(path[i]));
    f(i + 1, seed);
  }
}

bool hasAnchor(const Process::MessageNode& n)
{
  return n.anchor || ossia::any_of(n, hasAnchor);
}
}

void remapCopiedAnchors(QObject& root, const CopiedPaths& paths)
{
  // A paste can move many objects that many anchors point into: the copied
  // subtree an anchor is in is looked up by the hash of each of its prefixes.
  score::hash_map<std::size_t, std::vector<std::size_t>> byPrefix;
  byPrefix.reserve(paths.moved.size());
  for(std::size_t i = 0; i < paths.moved.size(); i++)
  {
    const auto& from = paths.moved[i].first.vec();
    forEachPrefixHash(from, [&](std::size_t n, std::size_t h) {
      if(n == from.size())
        byPrefix[h].push_back(i);
    });
  }

  LocalTree::AnchorMapping map = [&](const std::shared_ptr<const State::Anchor>& a)
      -> std::shared_ptr<const State::Anchor> {
    const auto& path = a->target.vec();
    // The first of the moved objects the anchor is in, as they are listed
    std::size_t found = paths.moved.size();
    forEachPrefixHash(path, [&](std::size_t n, std::size_t h) {
      if(auto it = byPrefix.find(h); it != byPrefix.end())
        for(auto i : it->second)
        {
          const auto& prefix = paths.moved[i].first.vec();
          if(i < found && prefix.size() == n
             && std::equal(prefix.begin(), prefix.end(), path.begin()))
            found = i;
        }
    });
    if(found == paths.moved.size())
      return paths.sameDocument ? a : nullptr;

    auto moved = paths.moved[found].second.vec();
    moved.insert(
        moved.end(), path.begin() + paths.moved[found].first.vec().size(), path.end());
    return std::make_shared<const State::Anchor>(
        State::Anchor{ObjectPath{std::move(moved)}, a->member});
  };

  forEach<StateModel>(root, [&](StateModel& state) {
    if(!hasAnchor(state.messages().rootNode()))
      return;
    auto tree = state.messages().rootNode();
    LocalTree::mapAnchors(tree, map);
    state.messages() = std::move(tree);
  });
  forEach<EventModel>(root, [&](EventModel& event) {
    auto e = event.condition();
    LocalTree::mapAnchors(e, map);
    event.setCondition(e);
  });
  forEach<TimeSyncModel>(root, [&](TimeSyncModel& sync) {
    auto e = sync.expression();
    LocalTree::mapAnchors(e, map);
    sync.setExpression(e);
  });
  forEach<Process::Port>(root, [&](Process::Port& port) {
    if(auto acc = port.address(); acc.address.anchor)
    {
      acc.address.anchor = map(acc.address.anchor);
      port.setAddress(acc);
    }
  });
}

void followPasted(const score::DocumentContext& ctx, const std::vector<QObject*>& pasted)
{
  auto tree = ctx.findPlugin<LocalTree::ScriptableTreeBase>();
  if(!tree)
    return;
  for(auto obj : pasted)
  {
    forEach<StateModel>(*obj, [&](StateModel& o) { tree->follow(o); });
    forEach<EventModel>(*obj, [&](EventModel& o) { tree->follow(o); });
    forEach<TimeSyncModel>(*obj, [&](TimeSyncModel& o) { tree->follow(o); });
    forEach<Process::Port>(*obj, [&](Process::Port& o) { tree->follow(o); });
  }
}

namespace Command
{
FollowPasted::FollowPasted(std::vector<ObjectPath> pasted)
    : m_pasted{std::move(pasted)}
{
}

void FollowPasted::undo(const score::DocumentContext& ctx) const { }

void FollowPasted::redo(const score::DocumentContext& ctx) const
{
  std::vector<QObject*> pasted;
  for(const auto& p : m_pasted)
    if(auto obj = p.try_find<QObject>(ctx))
      pasted.push_back(obj);
  followPasted(ctx, pasted);
}

void FollowPasted::serializeImpl(DataStreamInput& s) const
{
  s << m_pasted;
}

void FollowPasted::deserializeImpl(DataStreamOutput& s)
{
  s >> m_pasted;
}
}
}
