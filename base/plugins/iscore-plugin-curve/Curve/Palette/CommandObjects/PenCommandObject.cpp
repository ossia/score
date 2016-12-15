
#include <iscore/tools/Clamp.hpp>

#include "PenCommandObject.hpp"
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/model/path/Path.hpp>

namespace iscore
{
class CommandStackFacade;
} // namespace iscore

namespace Curve
{
PenCommandObject::PenCommandObject(
    Presenter* presenter, const iscore::CommandStackFacade& stack)
    : CommandObjectBase{presenter, stack}
    , m_segment{Id<SegmentModel>{}, nullptr}
{
}

void PenCommandObject::on_press()
{
  m_segment.setId(getSegmentId(m_startSegments));
  m_segment.setMinY(0);
  m_segment.setMaxY(1);
  m_originalPress = m_state->currentPoint;
  m_minPress = m_originalPress;
  m_maxPress = m_originalPress;
}

void PenCommandObject::move()
{
  // getSegmentId *has* to be prior to filterSegments for middleBegin /
  // middleEnd
  auto point_array_id = getSegmentId(m_startSegments);
  auto segts_tpl = filterSegments();
  auto& middleBegin = std::get<0>(segts_tpl);
  auto& middleEnd = std::get<1>(segts_tpl);
  auto& segts = std::get<2>(segts_tpl);

  auto dat_base = m_segment.toSegmentData();
  dat_base.id = point_array_id;
  dat_base.start = m_minPress;
  dat_base.end = m_maxPress;

  segts.reserve(segts.size() + 3);

  SegmentData* middle_begin_p{};
  SegmentData* middle_end_p{};
  if (middleBegin)
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = &segts.back();
  }
  if (middleEnd)
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = &segts.back();
  }

  segts.push_back(dat_base);
  SegmentData& dat = segts.back();

  if (middle_begin_p && middle_end_p && middle_begin_p->id == middle_end_p->id)
  {
    middle_end_p->id = getSegmentId(segts);
    for (auto& seg : segts)
    {
      if (seg.id == middle_end_p->following)
      {
        seg.previous = middle_end_p->id;
        break;
      }
    }
  }

  if (middle_begin_p)
  {
    SegmentData& seg = *middle_begin_p;
    seg.end = m_minPress;
    seg.following = dat.id;
    dat.previous = seg.id;
  }

  if (middle_end_p)
  {
    SegmentData& seg = *middle_end_p;
    seg.start = m_maxPress;
    seg.previous = dat.id;
    dat.following = seg.id;
  }

  submit(std::move(segts));
}

void PenCommandObject::release()
{
  auto segts_tpl = filterSegments();
  // First handle the case of a single point
  if (m_segment.points().size() == 1)
  {
    cancel();
  }
  else
  {
    release_n(std::move(segts_tpl));
  }
}

void PenCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_segment.reset();
}

void PenCommandObject::release_n(seg_tuple&& segts_tpl)
{
  m_segment.simplify(100);
  auto lin_segments = m_segment.toPowerSegments();

  auto& segts = std::get<2>(segts_tpl);

  segts.reserve(segts.size() + lin_segments.size() + 3);

  SegmentData* middle_begin_p{};
  SegmentData* middle_end_p{};

  if (auto& middleBegin = std::get<0>(segts_tpl))
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = &segts.back();
  }

  if (auto& middleEnd = std::get<1>(segts_tpl))
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = &segts.back();
  }

  const int first_inserted_lin = segts.size();
  const int N = lin_segments.size();

  { // Put the first one
    SegmentData& lin = lin_segments[0];
    lin.id = getSegmentId(segts);
    if (lin_segments.size() > 1)
      lin_segments[1].previous = lin.id;
    segts.push_back(std::move(lin));
  }

  if (N > 1)
  {
    // Main loop
    for (int i = 1; i < N - 1; i++)
    {
      SegmentData& lin = lin_segments[i];
      lin.id = getSegmentId(segts);

      segts[first_inserted_lin + i - 1].following = lin.id;

      lin_segments[i + 1].previous = lin.id;

      segts.push_back(std::move(lin));
    }

    { // Put the last one
      SegmentData& lin = lin_segments[N - 1];
      lin.id = getSegmentId(segts);
      segts[N - 2].following = lin.id;
      segts.push_back(std::move(lin));
    }
  }

  // Handle the case of the whole drawn curve being
  // contained in a single original segment
  if (middle_begin_p && middle_end_p && middle_begin_p->id == middle_end_p->id)
  {
    middle_end_p->id = getSegmentId(segts);
    for (auto& seg : segts)
    {
      if (seg.id == middle_begin_p->following)
      {
        seg.previous = middle_end_p->id;
        break;
      }
    }
  }

  // Link the new segments
  auto& first_lin = segts[first_inserted_lin];
  if (middle_begin_p)
  {
    SegmentData& seg = *middle_begin_p;
    seg.end = first_lin.start;
    seg.following = first_lin.id;
    first_lin.previous = seg.id;
  }

  auto& last_lin = segts.back();
  if (middle_end_p)
  {
    SegmentData& seg = *middle_end_p;
    seg.start = last_lin.end;
    seg.previous = last_lin.id;
    last_lin.following = seg.id;
  }

  submit(std::move(segts));
  m_dispatcher.commit();
  m_segment.reset();
}

std::
    tuple<optional<SegmentData>, optional<SegmentData>, std::vector<SegmentData>>
    PenCommandObject::filterSegments()
{
  auto x = m_state->currentPoint.x();
  if (x < m_minPress.x())
    m_minPress = m_state->currentPoint;
  if (x > m_maxPress.x())
    m_maxPress = m_state->currentPoint;

  m_segment.setStart(m_minPress);
  m_segment.setEnd(m_maxPress);
  m_segment.setMinX(m_minPress.x());
  m_segment.setMaxX(m_maxPress.x());
  m_segment.addPointUnscaled(x, m_state->currentPoint.y());

  auto segts = m_startSegments;

  // remove all segments that start after minPress and end after maxPress
  optional<SegmentData> middleBegin, middleEnd;
  for (auto it = segts.begin(); it != segts.end();)
  {
    const SegmentData& segt = *it;
    auto start_x = segt.start.x();
    auto end_x = segt.end.x();
    bool to_delete = false;
    if (start_x >= m_minPress.x() && end_x <= m_maxPress.x())
    {
      to_delete = true;
    }
    else
    {
      if (start_x <= m_minPress.x() && end_x >= m_minPress.x())
      {
        middleBegin = segt;
        to_delete = true;
      }
      if (start_x <= m_maxPress.x() && end_x >= m_maxPress.x())
      {
        middleEnd = segt;
        to_delete = true;
      }
    }

    if (to_delete)
    {
      it = segts.erase(it);
    }
    else
    {
      ++it;
    }
  }

  return std::make_tuple(middleBegin, middleEnd, std::move(segts));
}
}
