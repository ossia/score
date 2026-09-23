// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UpdateCurve.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/math/safe_math.hpp>

#include <QDebug>

namespace Curve
{
namespace
{
bool sameSegment(const SegmentData& a, const SegmentData& b)
{
  return a.start == b.start && a.end == b.end && a.previous == b.previous
         && a.following == b.following && a.type == b.type
         && a.specificSegmentData == b.specificSegmentData;
}

bool sameSegment(const SegmentModel& a, const SegmentData& b)
{
  return a.start() == b.start && a.end() == b.end && a.previous() == b.previous
         && a.following() == b.following && a.concreteKey() == b.type
         && a.specificDataEquals(b.specificSegmentData);
}

//! GUI thread only: reused across the updates of an edit.
struct UpdateScratch
{
  ossia::hash_map<int32_t, uint32_t> previous;
  ossia::hash_set<int32_t> next;
  ossia::hash_set<int32_t> changed;
  std::vector<uint8_t> seen;
  std::vector<CurveChange> changes;
  std::vector<Id<SegmentModel>> removed;
  std::vector<const SegmentData*> upserted;
};

UpdateScratch& updateScratch() noexcept
{
  static UpdateScratch s;
  return s;
}
}

UpdateCurve::UpdateCurve(const Model& model, const std::vector<SegmentData>& segments)
    : m_model{model}
{
  if(!setChanges(model, segments))
    qDebug() << "Curve error: keeping the current curve";
}

void UpdateCurve::update(const Model& model, const std::vector<SegmentData>& segments)
{
  // Called on each move of a drag: a refused step keeps the last valid curve.
  setChanges(model, segments);
}

namespace
{
bool validSegment(const SegmentData& d) noexcept
{
  return ossia::safe_isfinite(d.start.x()) && ossia::safe_isfinite(d.start.y())
         && ossia::safe_isfinite(d.end.x()) && ossia::safe_isfinite(d.end.y())
         && d.start.x() <= d.end.x();
}

//! Same segment, same links: the chains are unchanged.
bool sameLinks(const SegmentData& a, const SegmentData& b) noexcept
{
  return a.previous == b.previous && a.following == b.following && a.type == b.type;
}
}

bool UpdateCurve::setChanges(const Model& model, const std::vector<SegmentData>& next)
{
  auto& s = updateScratch();

  // What each segment was before this command: recorded in the previous
  // changes, or else as the model has it, since this command did not touch it.
  s.previous.clear();
  s.previous.reserve(m_changes.size());
  std::size_t in_model_through_previous = 0;
  for(uint32_t i = 0; i < m_changes.size(); i++)
  {
    s.previous.emplace(m_changes[i].id.val(), i);
    if(m_changes[i].after)
      in_model_through_previous++;
  }
  s.seen.assign(m_changes.size(), 0);
  s.changes.clear();

  // Whether the chains change: only then must the whole curve be checked.
  bool structural = false;

  const auto& sorted = model.sortedSegments();
  const auto& segments = model.segments();
  std::size_t matched = 0;
  std::size_t cursor = 0;
  for(std::size_t i = 0; i < next.size(); i++)
  {
    const auto& d = next[i];

    if(auto it = s.previous.find(d.id.val()); it != s.previous.end())
    {
      // Seen twice: a duplicate id, which only the full check refuses.
      structural |= s.seen[it->second];
      s.seen[it->second] = 1;
      const auto& before = m_changes[it->second].before;
      if(!before || !sameSegment(*before, d))
      {
        structural |= !before || !sameLinks(*before, d);
        s.changes.push_back({d.id, before, d});
      }
      continue;
    }

    // An edit mostly keeps the model's order, minus what it removes: walk
    // both in step, skipping the model's segments that come before d.
    const SegmentModel* cur{};
    while(cursor < sorted.size() && sorted[cursor]->id() != d.id
          && sorted[cursor]->start().x() < d.start.x())
      cursor++;
    if(cursor < sorted.size() && sorted[cursor]->id() == d.id)
      cur = sorted[cursor++];
    else if(auto it = segments.find(d.id); it != segments.end())
    {
      // Out of order, or a duplicate id: the counts below cannot tell.
      structural = true;
      cur = &*it;
    }

    if(!cur)
    {
      structural = true;
      s.changes.push_back({d.id, std::nullopt, d});
      continue;
    }

    matched++;
    if(!sameSegment(*cur, d))
    {
      auto before = cur->toSegmentData();
      structural |= !sameLinks(before, d);
      s.changes.push_back({d.id, std::move(before), d});
    }
  }

  // What existed before and is not in `next`.
  if(matched + in_model_through_previous != segments.size())
  {
    structural = true;
    s.next.clear();
    s.next.reserve(next.size());
    for(const auto& d : next)
      s.next.insert(d.id.val());
    for(const auto& seg : segments)
    {
      const auto id = seg.id().val();
      if(!s.next.contains(id) && !s.previous.contains(id))
        s.changes.push_back({seg.id(), seg.toSegmentData(), std::nullopt});
    }
  }
  for(uint32_t i = 0; i < m_changes.size(); i++)
  {
    const auto& c = m_changes[i];
    if(!s.seen[i] && c.before)
    {
      structural = true;
      s.changes.push_back({c.id, c.before, std::nullopt});
    }
  }

  // The current curve is valid: when the chains stay as they are, only what
  // changed needs checking.
  const bool valid = structural
                         ? isValidCurve(next)
                         : ossia::all_of(s.changes, [](const CurveChange& c) {
                             return validSegment(*c.after);
                           });
  if(!valid)
  {
    s.changes.clear();
    return false;
  }

  // The model is where the previous changes left it: whatever they touched
  // and the new ones do not must go back to its state before the command.
  s.changed.clear();
  s.changed.reserve(s.changes.size());
  for(const auto& c : s.changes)
    s.changed.insert(c.id.val());
  for(const auto& c : m_changes)
    if(!s.changed.contains(c.id.val()))
      m_revert.push_back({c.id, c.after, c.before});

  std::swap(m_changes, s.changes);
  s.changes.clear();
  return true;
}

void UpdateCurve::apply(const score::DocumentContext& ctx, bool forward) const
{
  auto& curve = m_model.find(ctx);
  auto& s = updateScratch();
  s.removed.clear();
  s.upserted.clear();
  auto add = [&](const CurveChange& c, bool fwd) {
    const auto& target = fwd ? c.after : c.before;
    if(target)
      s.upserted.push_back(&*target);
    else
      s.removed.push_back(c.id);
  };
  for(const auto& c : m_revert)
    add(c, true);
  for(const auto& c : m_changes)
    add(c, forward);
  curve.applyChanges(s.removed, s.upserted);
  m_revert.clear();
}

void UpdateCurve::undo(const score::DocumentContext& ctx) const
{
  apply(ctx, false);
}

void UpdateCurve::redo(const score::DocumentContext& ctx) const
{
  apply(ctx, true);
}

// Tells the change list from the curves before and after that older
// versions wrote, as the vector's count, in crash-restore files.
static constexpr int32_t changes_format = -1;

void UpdateCurve::serializeImpl(DataStreamInput& s) const
{
  s << m_model << changes_format << (int32_t)m_changes.size();
  for(const auto& c : m_changes)
  {
    s << c.id << bool(c.before) << bool(c.after);
    if(c.before)
      s << *c.before;
    if(c.after)
      s << *c.after;
  }
}

void UpdateCurve::deserializeImpl(DataStreamOutput& s)
{
  int32_t n{};
  s >> m_model;
  s.stream >> n;
  m_changes.clear();
  if(n != changes_format)
  {
    // The whole curve before, then after.
    std::vector<SegmentData> before(std::max(n, 0)), after;
    for(auto& d : before)
      s >> d;
    {
      DataStreamWriter w{s.stream.device()};
      SCORE_DEBUG_CHECK_DELIMITER2(w);
    }
    s >> after;

    ossia::hash_map<int32_t, const SegmentData*> old;
    old.reserve(before.size());
    for(const auto& d : before)
      old.emplace(d.id.val(), &d);
    for(const auto& d : after)
    {
      auto it = old.find(d.id.val());
      if(it == old.end())
        m_changes.push_back({d.id, std::nullopt, d});
      else
      {
        m_changes.push_back({d.id, *it->second, d});
        old.erase(it);
      }
    }
    for(const auto& d : before)
      if(old.contains(d.id.val()))
        m_changes.push_back({d.id, d, std::nullopt});
    return;
  }

  s.stream >> n;
  m_changes.resize(std::max(n, 0));
  for(auto& c : m_changes)
  {
    bool has_before{}, has_after{};
    s >> c.id >> has_before >> has_after;
    if(has_before)
      s >> c.before.emplace();
    if(has_after)
      s >> c.after.emplace();
  }
}
}
