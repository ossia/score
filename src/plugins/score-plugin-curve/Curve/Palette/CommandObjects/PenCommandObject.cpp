// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "PenCommandObject.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Clamp.hpp>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
/*
void checkCoherent(const std::vector<SegmentData>& vec)
{
  std::cerr << "\n";
  for(int i = 0; i < vec.size(); i++)
  {
    if(vec[i].following)
    {
      auto next = ossia::find_if(vec, [&] (const SegmentData& d) { return d.id
== vec[i].following; }); SCORE_ASSERT(next != vec.end()); qDebug()  << i
                << "actual: " << vec[i].id.val() << "\n"
                << "expected: " << (next->previous ?
QString::number(next->previous->val()) : QString("none"))
                << vec[i].following->val();
      SCORE_ASSERT(next->previous == vec[i].id);
    }
    if(vec[i].previous)
    {
      auto prev = ossia::find_if(vec, [&] (const SegmentData& d) { return d.id
== vec[i].previous; }); SCORE_ASSERT(prev != vec.end()); qDebug()  << i
                << "actual: " <<  vec[i].id.val() << "\n"
                << "expected: " << (prev->following ?
QString::number(prev->following->val()) : QString("none"))
                << vec[i].previous->val();
      SCORE_ASSERT(prev->following == vec[i].id);
    }
  }
}
*/
PenCommandObject::PenCommandObject(Presenter* presenter, const score::CommandStackFacade& stack)
    : CommandObjectBase{presenter, stack}, m_segment{Id<SegmentModel>{}, nullptr}
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

  std::optional<std::size_t> middle_begin_p{};
  std::optional<std::size_t> middle_end_p{};
  if (middleBegin)
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = segts.size() - 1;
  }
  if (middleEnd)
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = segts.size() - 1;
  }

  segts.push_back(dat_base);
  SegmentData& dat = segts.back();

  if (middle_begin_p && middle_end_p && segts[*middle_begin_p].id == segts[*middle_end_p].id)
  {
    segts[*middle_end_p].id = getSegmentId(segts);
    for (auto& seg : segts)
    {
      if (seg.id == segts[*middle_end_p].following)
      {
        seg.previous = segts[*middle_end_p].id;
        break;
      }
    }
  }

  if (middle_begin_p)
  {
    SegmentData& seg = segts[*middle_begin_p];
    seg.end = m_minPress;
    seg.following = dat.id;
    dat.previous = seg.id;
  }

  if (middle_end_p)
  {
    SegmentData& seg = segts[*middle_end_p];
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

  std::optional<std::size_t> middle_begin_p{};
  std::optional<std::size_t> middle_end_p{};

  if (auto& middleBegin = std::get<0>(segts_tpl))
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = segts.size() - 1;
  }

  if (auto& middleEnd = std::get<1>(segts_tpl))
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = segts.size() - 1;
  }

  const std::size_t first_inserted_lin = segts.size();
  const std::size_t N = lin_segments.size();

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
    for (std::size_t i = 1; i < N - 1; i++)
    {
      SegmentData& lin = lin_segments[i];
      lin.id = getSegmentId(segts);

      segts[first_inserted_lin + i - 1].following = lin.id;

      lin_segments[i + 1].previous = lin.id;

      segts.push_back(std::move(lin));
    }

    { // Put the last one
      SegmentData& lin = lin_segments.back();
      lin.id = getSegmentId(segts);
      segts.back().following = lin.id;
      segts.push_back(std::move(lin));
    }
  }

  // Handle the case of the whole drawn curve being
  // contained in a single original segment
  if (middle_begin_p && middle_end_p && segts[*middle_begin_p].id == segts[*middle_end_p].id)
  {
    auto& mb = segts[*middle_begin_p];
    auto& me = segts[*middle_end_p];
    me.id = getSegmentId(segts);
    for (auto& seg : segts)
    {
      if (seg.id == mb.following)
      {
        seg.previous = me.id;
        break;
      }
    }
  }

  // Link the new segments
  auto& first_lin = segts[first_inserted_lin];
  if (middle_begin_p)
  {
    SegmentData& seg = segts[*middle_begin_p];
    seg.end = first_lin.start;
    seg.following = first_lin.id;
    first_lin.previous = seg.id;
  }

  auto& last_lin = segts.back();
  if (middle_end_p)
  {
    SegmentData& seg = segts[*middle_end_p];
    seg.start = last_lin.end;
    seg.previous = last_lin.id;
    last_lin.following = seg.id;
  }

  //  checkCoherent(segts);
  submit(std::move(segts));
  m_dispatcher.commit();
  m_segment.reset();
}

std::tuple<std::optional<SegmentData>, std::optional<SegmentData>, std::vector<SegmentData>>
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
  std::optional<SegmentData> middleBegin, middleEnd;
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
