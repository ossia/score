#include "CurveEditor.hpp"

#include <Process/Process.hpp>

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
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
  Curve::SegmentIdAllocator ids{existing};
  for(auto& seg : paste_segts)
    seg.id = ids.next();
  segments.reserve(existing.size() + paste_segts.size() + 1);
  for(auto seg : existing)
  {
    if(seg.end.x() <= p0)
    {
      segments.push_back(std::move(seg));
    }
    else if(seg.start.x() < p0)
    {
      // Cut at the start of the paste: dropped if nothing is left of it.
      Curve::setSegmentExtent(seg, seg.start, first_pasted.start);
      if(seg.end.x() > seg.start.x())
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
      // Cut on both sides: its first half already has its id.
      if(seg.start.x() < p0)
        seg.id = ids.next();
      Curve::setSegmentExtent(seg, last_pasted.end, seg.end);
      if(seg.end.x() > seg.start.x())
        segments.push_back(std::move(seg));
    }
  }

  // Finally relink everything. The segments already there keep their ids,
  // so that only what the paste changes is updated in the curve.
  const int N = std::ssize(segments);
  for(int i = 0; i < N; i++)
  {
    auto& seg = segments[i];
    seg.previous = i > 0 ? OptionalId<Curve::SegmentModel>{segments[i - 1].id}
                         : std::nullopt;
    seg.following = i + 1 < N ? OptionalId<Curve::SegmentModel>{segments[i + 1].id}
                              : std::nullopt;
    if(i + 1 < N)
      seg.end = segments[i + 1].start;
  }

  CommandDispatcher<>{ctx.commandStack}.submit(new UpdateCurve{m, segments});
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

  // A selected point takes its two segments with it; the first and last points
  // of a chain are not removed.
  ossia::hash_set<int32_t> segmentsToDelete;
  for(const auto& elt : s)
  {
    if(auto point = qobject_cast<const PointModel*>(elt.data()))
    {
      if(point->previous() && point->following())
      {
        segmentsToDelete.insert(point->previous()->val());
        segmentsToDelete.insert(point->following()->val());
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

  CommandDispatcher<>{ctx.commandStack}.submit(
      new UpdateCurve{*cm, removeSegments(*cm, segmentsToDelete, true)});
  return true;
}

}
