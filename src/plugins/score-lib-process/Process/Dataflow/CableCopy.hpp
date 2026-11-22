#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/ptr_set.hpp>

namespace Dataflow
{
using SerializedCables = std::vector<std::pair<Id<Process::Cable>, Process::CableData>>;

//! Moves a copied cable's end to the copy of the process it starts at: `ids`
//! gives the copy's identifier for the original's. False when that process
//! was not copied.
inline bool
relocateToCopy(ObjectPath& path, const std::vector<std::pair<int32_t, int32_t>>& ids)
{
  auto& vec = path.vec();
  if(vec.empty())
    return false;
  for(auto [old_id, new_id] : ids)
    if(vec.front().id() == old_id)
    {
      vec.front() = ObjectIdentifier{vec.front().objectName(), new_id};
      return true;
    }
  return false;
}

namespace detail
{
inline Process::CableData& cableData(Process::CableData& c) noexcept { return c; }
inline Process::CableData& cableData(SerializedCables::value_type& c) noexcept
{
  return c.second;
}
}

//! The copied cables both ends of which `relocate` moves to the copies, under
//! identifiers new among the document's `existing` cables. The others are
//! left out: an end that kept its path would land on whatever has it here.
template <typename Cables, typename Relocate, typename Existing>
SerializedCables
keepCopiedCables(Cables&& cables, Relocate&& relocate, const Existing& existing)
{
  std::vector<Process::CableData> kept;
  kept.reserve(cables.size());
  for(auto& c : cables)
  {
    auto& cd = detail::cableData(c);
    if(relocate(cd.source.unsafePath()) && relocate(cd.sink.unsafePath()))
      kept.push_back(std::move(cd));
  }

  // One identifier each: the document's cables do not change meanwhile
  const auto ids = getStrongIdRange<Process::Cable>(kept.size(), existing);
  SerializedCables res;
  res.reserve(kept.size());
  for(std::size_t i = 0; i < kept.size(); i++)
    res.emplace_back(ids[i], std::move(kept[i]));
  return res;
}
}
namespace Process
{

inline bool verifyAndUpdateIfChildOf(ObjectPath& path, const ObjectPath& parent)
{
  auto parent_n = parent.vec().size();
  auto path_n = path.vec().size();
  if(parent_n >= path_n)
    return false;
  for(std::size_t i = 0; i < parent_n; i++)
  {
    if(!(path.vec()[i] == parent.vec()[i]))
      return false;
  }

  SCORE_ASSERT(parent_n > 1);
  path.vec().erase(path.vec().begin(), path.vec().begin() + parent_n - 1);
  return true;
}

template <typename T>
bool verifyAndUpdateIfChildOf(Process::CableData& path, const std::vector<Path<T>>& vec)
{
  bool source_ok = false;
  for(const auto& parent : vec)
  {
    if(verifyAndUpdateIfChildOf(path.source.unsafePath(), parent.unsafePath()))
    {
      source_ok = true;
      break;
    }
  }
  if(!source_ok)
    return false;

  for(const auto& parent : vec)
  {
    if(verifyAndUpdateIfChildOf(path.sink.unsafePath(), parent.unsafePath()))
    {
      return true;
    }
  }
  // must not happen: the sink is already guaranteed to be a child of an
  // interval since we look for all the inlets
  SCORE_ABORT;
}

template <typename T>
Dataflow::SerializedCables cablesToCopy(
    const std::vector<T*>& array,
    const std::vector<Path<std::remove_const_t<T>>>& siblings,
    const score::DocumentContext& ctx)
{
  // For every cable, if both ends are in one of the elements or child elements
  // currently selected, we copy them.
  // Note: ids / cable paths have to be updated of course.
  Dataflow::SerializedCables copiedCables;
  ossia::ptr_set<Process::Inlet*> ins;
  for(auto itv : array)
  {
    auto child_ins = itv->template findChildren<Process::Inlet*>();
    ins.insert(child_ins.begin(), child_ins.end());
  }

  for(auto inl : ins)
  {
    for(const auto& c_inl : inl->cables())
    {
      if(Process::Cable* cable = c_inl.try_find(ctx))
      {
        auto cd = cable->toCableData();
        if(verifyAndUpdateIfChildOf(cd, siblings))
        {
          copiedCables.push_back({cable->id(), cd});
        }
      }
    }
  }

  return copiedCables;
}

template <typename T>
Dataflow::SerializedCables
cablesToCopy(const std::vector<T*>& array, const score::DocumentContext& ctx)
{
  std::vector<Path<std::remove_const_t<T>>> siblings;
  siblings.reserve(array.size());
  for(auto ptr : array)
  {
    siblings.emplace_back(*ptr);
  }
  return cablesToCopy(array, siblings, ctx);
}
}
