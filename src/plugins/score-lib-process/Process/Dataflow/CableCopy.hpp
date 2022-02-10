#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <ossia/detail/ptr_set.hpp>

namespace Dataflow
{
using SerializedCables
    = std::vector<std::pair<Id<Process::Cable>, Process::CableData>>;
}
namespace Process
{

inline
bool verifyAndUpdateIfChildOf(ObjectPath& path, const ObjectPath& parent)
{
  auto parent_n = parent.vec().size();
  auto path_n = path.vec().size();
  if (parent_n >= path_n)
    return false;
  for (std::size_t i = 0; i < parent_n; i++)
  {
    if (!(path.vec()[i] == parent.vec()[i]))
      return false;
  }

  SCORE_ASSERT(parent_n > 1);
  path.vec().erase(path.vec().begin(), path.vec().begin() + parent_n - 1);
  return true;
}

template <typename T>
bool verifyAndUpdateIfChildOf(
    Process::CableData& path,
    const std::vector<Path<T>>& vec)
{
  bool source_ok = false;
  for (const auto& parent : vec)
  {
    if (verifyAndUpdateIfChildOf(
            path.source.unsafePath(), parent.unsafePath()))
    {
      source_ok = true;
      break;
    }
  }
  if (!source_ok)
    return false;

  for (const auto& parent : vec)
  {
    if (verifyAndUpdateIfChildOf(path.sink.unsafePath(), parent.unsafePath()))
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
  for (auto itv : array)
  {
    auto child_ins = itv->template findChildren<Process::Inlet*>();
    ins.insert(child_ins.begin(), child_ins.end());
  }

  for (auto inl : ins)
  {
    for (const auto& c_inl : inl->cables())
    {
      if (Process::Cable* cable = c_inl.try_find(ctx))
      {
        auto cd = cable->toCableData();
        if (verifyAndUpdateIfChildOf(cd, siblings))
        {
          copiedCables.push_back({cable->id(), cd});
        }
      }
    }
  }

  return copiedCables;
}

template <typename T>
Dataflow::SerializedCables cablesToCopy(
    const std::vector<T*>& array,
    const score::DocumentContext& ctx)
{
  std::vector<Path<std::remove_const_t<T>>> siblings;
  siblings.reserve(array.size());
  for(auto ptr : array) {
    siblings.emplace_back(*ptr);
  }
  return cablesToCopy(array, siblings, ctx);
}
}
