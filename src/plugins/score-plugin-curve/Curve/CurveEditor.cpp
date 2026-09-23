#include "CurveEditor.hpp"

#include <Process/Process.hpp>

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>

#include <core/document/Document.hpp>

#include <QMimeData>

#include <set>
namespace Curve
{

bool CurveEditor::copy(
    JSONReader& r, const Selection& s, const score::DocumentContext& ctx)
{
  if(s.empty())
    return false;
  std::vector<Curve::SegmentModel*> segments;
  for(const auto& obj : s)
  {
    if(auto s = qobject_cast<Curve::SegmentModel*>(obj.data()))
      segments.push_back(s);
    else if(qobject_cast<Curve::PointModel*>(obj.data()))
      continue;
    else
      return false;
  }

  if(segments.empty())
    return true;

  auto model = qobject_cast<Curve::Model*>(segments[0]->parent());
  if(!model)
    return true;

  std::vector<Curve::SegmentData> dat;

  for(Curve::SegmentModel* seg : segments)
    dat.push_back(seg->toSegmentData());
  ossia::sort(dat);

  r.stream.StartObject();
  r.obj["Segments"] = dat;
  r.stream.EndObject();

  return true;
}

bool CurveEditor::paste(
    QPoint pos, QObject* focusedObject, const QMimeData& mime,
    const score::DocumentContext& ctx)
{
  if(!focusedObject)
    return false;

  auto curve = focusedObject->findChild<Curve::Presenter*>();
  if(!curve)
    return false;

  auto obj = readJson(mime.data("text/plain"));
  if(!obj.IsObject())
    return false;

  auto seg_it = obj.FindMember("Segments");
  if(seg_it == obj.MemberEnd() || !seg_it->value.IsArray())
    return false;

  auto& p = *curve;
  auto& v = p.view();
  auto& m = p.model();
  auto pt = mapPointToItem(pos, v);
  if(!pt)
    return true;

  const double w = v.boundingRect().width();
  if(w <= 0.)
    return true;
  const double x = pt->x() / w;

  // Those are ordered
  auto paste_segts = JsonValue{seg_it->value}.to<std::vector<Curve::SegmentData>>();
  if(paste_segts.empty())
    return true;

  // Move the segments to the right position
  double seg_delta = -paste_segts[0].start.x() + x;
  for(auto& seg : paste_segts)
  {
    seg.start.rx() += seg_delta;
    seg.end.rx() += seg_delta;
  }

  // What we have in the pasted-to curve, cut around the pasted range. A
  // segment that spans the whole range is kept on both sides of it.
  const auto& first_pasted = paste_segts.front();
  const auto& last_pasted = paste_segts.back();
  const double p0 = first_pasted.start.x();
  const double p1 = last_pasted.end.x();

  std::vector<Curve::SegmentData> segments;
  const auto existing = orderedSegments(m);
  segments.reserve(existing.size() + paste_segts.size() + 1);
  for(auto seg : existing)
  {
    if(seg.end.x() <= p0)
    {
      segments.push_back(std::move(seg));
    }
    else if(seg.start.x() < p0)
    {
      seg.end = first_pasted.start;
      segments.push_back(std::move(seg));
    }
  }

  segments.insert(segments.end(), paste_segts.begin(), paste_segts.end());

  for(auto seg : existing)
  {
    if(seg.start.x() >= p1)
    {
      segments.push_back(std::move(seg));
    }
    else if(seg.end.x() > p1)
    {
      seg.start = last_pasted.end;
      segments.push_back(std::move(seg));
    }
  }

  // Do some clean-up, remove empty segments

  for(auto it = segments.begin(); it != segments.end();)
  {
    auto& seg = *it;
    if(std::abs(seg.end.x() - seg.start.x()) < 0.001)
    {
      it = segments.erase(it);
      continue;
    }
    if(seg.end.x() <= seg.start.x())
    {
      it = segments.erase(it);
      continue;
    }

    ++it;
  }

  if(segments.empty())
    return true;

  // Finally relink everything
  segments.front().id = Id<Curve::SegmentModel>{0};
  segments.front().previous = std::nullopt;

  segments.back().id = Id<Curve::SegmentModel>{int(std::ssize(segments) - 1)};
  segments.back().following = std::nullopt;

  int N = std::ssize(segments);
  if(N >= 2)
  {
    int i = 1;
    for(; i < N - 1; i++)
    {
      segments[i].id = Id<Curve::SegmentModel>{i};
      segments[i].previous = segments[i - 1].id;
      segments[i - 1].following = segments[i].id;
      segments[i - 1].end = segments[i].start;
    }

    i = N - 2;

    {
      segments.back().previous = segments[i].id;
      segments[i].following = segments.back().id;
      segments[i].end = segments.back().start;
    }
  }

  CommandDispatcher<>{ctx.commandStack}.submit(new UpdateCurve{m, std::move(segments)});
  return true;
}

bool CurveEditor::remove(const Selection& s, const score::DocumentContext& ctx)
{
  // Check if we are focusing a curve
  auto focused_process
      = qobject_cast<const Process::ProcessModel*>(ctx.document.focusManager().get());
  if(!focused_process)
    return false;

  auto cm
      = focused_process->findChild<Curve::Model*>(QString{}, Qt::FindDirectChildrenOnly);
  if(!cm)
    return false;

  {
    auto& m_model = *cm;
    // We remove all that is selected,
    // And set the bounds correctly
    ossia::hash_set<Id<SegmentModel>> segmentsToDelete;

    // First find the segments that will be deleted.
    // If a point is selected, the segments linked to that point
    // will be deleted, too.
    for(const auto& elt : s)
    {
      if(auto point = qobject_cast<const PointModel*>(elt.data()))
      {
        if(point->previous() && point->following())
        {
          segmentsToDelete.insert(*point->previous());
          segmentsToDelete.insert(*point->following());
        }
      }
      else if(qobject_cast<const SegmentModel*>(elt.data()))
      {
        continue;
      }
      else
      {
        // Not a point nor a segment: we likely
        // selected a curve process in order to delete it
        return false;
      }
    }

    if(segmentsToDelete.empty())
      return true;

    double x0 = 0;
    double y0 = 0;
    double x1 = 1;
    double y1 = 1;
    bool firstRemoved = false;
    bool lastRemoved = false;
    // Then remove
    auto newSegments = m_model.toCurveData();
    {
      // First look for the start and end segments
      {
        for(auto& seg : newSegments)
        {
          if(ossia::contains(segmentsToDelete, seg.id))
          {
            if(!seg.previous)
            {
              firstRemoved = true;
              x0 = seg.start.x();
              y0 = seg.start.y();
            }
            if(!seg.following)
            {
              lastRemoved = true;
              x1 = seg.end.x();
              y1 = seg.end.y();
            }
          }
        }
      }

      // Then set the others
      auto it = newSegments.begin();
      while(it != newSegments.end())
      {
        if(ossia::contains(segmentsToDelete, it->id))
        {
          if(it->previous)
          {
            auto prev_it = ossia::find_if(newSegments, [&](const SegmentData& d) {
              return d.id == *it->previous;
            });
            if(prev_it != newSegments.end())
              prev_it->following = OptionalId<SegmentModel>{};
          }
          if(it->following)
          {
            auto next_it = ossia::find_if(newSegments, [&](const SegmentData& d) {
              return d.id == *it->following;
            });
            if(next_it != newSegments.end())
              next_it->previous = OptionalId<SegmentModel>{};
          }
          it = newSegments.erase(it);
          continue;
        }

        if(it->previous && ossia::contains(segmentsToDelete, it->previous))
          it->previous = OptionalId<SegmentModel>{};
        if(it->following && ossia::contains(segmentsToDelete, it->following))
          it->following = OptionalId<SegmentModel>{};

        it++;
      }
    }

    // Recreate if appropriate
    {
      // Find the "holes" in the new segment list.
      ossia::sort(newSegments, [](const SegmentData& s1, const SegmentData& s2) {
        return s1.x() < s2.x();
      });

      // First if there is no segments, we recreate one.
      if(newSegments.empty())
      {
        SegmentData d;
        d.start = QPointF{0, y0};
        d.end = QPointF{1, y1};
        d.id = getSegmentId(newSegments);
        d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
        d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
        newSegments.push_back(d);
      }
      else
      {
        if(firstRemoved)
        {
          // Recreate a segment from x = 0 to the beginning of the first segment.
          auto it = newSegments.begin();

          // Create a new segment
          SegmentData d;
          d.start = QPointF{x0, y0};
          d.end = it->start;
          d.following = it->id;
          d.id = getSegmentId(newSegments);
          d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
          d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
          it->previous = d.id;

          newSegments.insert(it, d);
        }

        if(lastRemoved)
        {
          // Recreate a segment from x = 0 to the end of the last segment.
          auto it = newSegments.rbegin();

          // Create a new segment
          SegmentData d;
          d.end = QPointF{x1, y1};
          d.start = it->end;
          d.previous = it->id;
          d.id = getSegmentId(newSegments);
          d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
          d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
          it->following = d.id;

          newSegments.insert(newSegments.end(), d);
        }
      }

      // Then try to fill the holes
      auto it = newSegments.begin();
      for(; it != newSegments.end();)
      {
        // Check if it's the last segment
        auto next = it + 1;
        if(next == newSegments.end())
          break;

        if(it->following)
        {
          it = next;
        }
        else
        {
          // Create a new segment
          SegmentData d;
          d.start = it->end;
          d.end = next->start;
          d.previous = it->id;
          d.following = next->id;
          d.id = getSegmentId(newSegments);
          d.type = Metadata<ConcreteKey_k, DefaultCurveSegmentModel>::get();
          d.specificSegmentData = QVariant::fromValue(DefaultCurveSegmentData{});
          it->following = d.id;
          next->previous = d.id;

          it = newSegments.insert(it, d);
          // it is now at the position of the new segment.

          ++it;
          // it is now at the position of next
        }
      }
    }

    // Apply the changes.
    CommandDispatcher<>{ctx.commandStack}.submit(
        new UpdateCurve{m_model, std::move(newSegments)});
  }
  return true;
}

}
