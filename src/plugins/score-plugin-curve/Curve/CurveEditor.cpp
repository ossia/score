#include "CurveEditor.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Process/Process.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <set>
namespace Curve
{

bool CurveEditor::copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx)
{
  SCORE_TODO;
  return true;
}

bool CurveEditor::paste(QPoint pos, const QMimeData& mime, const score::DocumentContext& ctx)
{
  SCORE_TODO;
  return true;
}

bool CurveEditor::remove(const Selection& s, const score::DocumentContext& ctx)
{
  // Check if we are focusing a curve
  auto focused_process = qobject_cast<const Process::ProcessModel*>(
      ctx.document.focusManager().get());
  if(!focused_process)
    return false;

  auto cm = focused_process->findChild<Curve::Model*>();
  if(!cm)
    return false;

  {
    auto& m_model = *cm;
    // We remove all that is selected,
    // And set the bounds correctly
    std::set<Id<SegmentModel>> segmentsToDelete;

    // First find the segments that will be deleted.
    // If a point is selected, the segments linked to that point
    // will be deleted, too.
    const auto& c = m_model.selectedChildren();
    for (const auto& elt : c)
    {
      if (auto point = qobject_cast<const PointModel*>(elt.data()))
      {
        if (point->previous() && point->following())
        {
          segmentsToDelete.insert(*point->previous());
          segmentsToDelete.insert(*point->following());
        }
      }

      /*
      if(auto segmt = qobject_cast<const SegmentModel*>(elt.data()))
      {
          segmentsToDelete.insert(segmt->id());
      }
      */
    }

    if (segmentsToDelete.empty())
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
        for (auto& seg : newSegments)
        {
          if (ossia::contains(segmentsToDelete, seg.id))
          {
            if (!seg.previous)
            {
              firstRemoved = true;
              x0 = seg.start.x();
              y0 = seg.start.y();
            }
            if (!seg.following)
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
      while (it != newSegments.end())
      {
        if (ossia::contains(segmentsToDelete, it->id))
        {
          if (it->previous)
          {
            auto prev_it
                = ossia::find_if(newSegments, [&](const SegmentData& d) {
                    return d.id == *it->previous;
                  });
            if (prev_it != newSegments.end())
              prev_it->following = OptionalId<SegmentModel>{};
          }
          if (it->following)
          {
            auto next_it
                = ossia::find_if(newSegments, [&](const SegmentData& d) {
                    return d.id == *it->following;
                  });
            if (next_it != newSegments.end())
              next_it->previous = OptionalId<SegmentModel>{};
          }
          it = newSegments.erase(it);
          continue;
        }

        if (it->previous && ossia::contains(segmentsToDelete, it->previous))
          it->previous = OptionalId<SegmentModel>{};
        if (it->following && ossia::contains(segmentsToDelete, it->following))
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
      if (newSegments.empty())
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
        if (firstRemoved)
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

        if (lastRemoved)
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
      for (; it != newSegments.end();)
      {
        // Check if it's the last segment
        auto next = it + 1;
        if (next == newSegments.end())
          break;

        if (it->following)
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
    CommandDispatcher<>{ctx.commandStack}.submit(new UpdateCurve{m_model, std::move(newSegments)});
  }
  /*
  if (s.size() == 1)
  {
    auto first = s.begin()->data();
    if (auto model = qobject_cast<const Process::ProcessModel*>(first))
    {
      if (auto parent = qobject_cast<Model*>(model->parent()))
      {
        auto f = [&ctx, parent, model] {
          CommandDispatcher<>{ctx.commandStack}.submit<RemoveNode>(
              *parent, *model);
        };
        ossia::qt::run_async(qApp, f);
        return true;
      }
    }
  }
  */
  return false;
}

}
