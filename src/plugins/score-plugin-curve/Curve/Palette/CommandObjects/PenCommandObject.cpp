// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "PenCommandObject.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Settings/CurveSettingsModel.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Clamp.hpp>

namespace Curve
{
PenCommandObject::PenCommandObject(
    Presenter* presenter, const score::CommandStackFacade& stack)
    : CommandObjectBase{presenter->model(), presenter, stack}
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
  auto& middleBegin = segts_tpl.middleBegin;
  auto& middleEnd = segts_tpl.middleEnd;
  auto& segts = segts_tpl.segments;
  checkValidity(segts);

  if(std::abs(m_maxPress.x() - m_minPress.x()) < 1e-8)
    return;

  auto dat_base = m_segment.toSegmentData();
  dat_base.id = point_array_id;
  dat_base.start = m_minPress;
  dat_base.end = m_maxPress;

  segts.reserve(segts.size() + 3);

  std::optional<std::size_t> middle_begin_p{};
  std::optional<std::size_t> middle_end_p{};
  if(middleBegin)
  {
    if(middleBegin->start.x() >= m_minPress.x())
    {
      dat_base.previous = middleBegin->previous;
      if(dat_base.previous)
      {
        for(auto& other : segts)
        {
          if(other.id == dat_base.previous)
          {
            other.following = dat_base.id;
            other.end = dat_base.start;
            break;
          }
        }
      }
      middleBegin = std::nullopt;
    }
    else
    {
      segts.push_back(*std::move(middleBegin));
      middle_begin_p = segts.size() - 1;
    }
  }
  if(middleEnd)
  {
    if(middleEnd->end.x() <= m_maxPress.x())
    {
      dat_base.following = middleEnd->following;
      if(dat_base.following)
      {
        for(auto& other : segts)
        {
          if(other.id == dat_base.following)
          {
            other.previous = dat_base.id;
            other.start = dat_base.end;
            break;
          }
        }
      }
      middleEnd = std::nullopt;
    }
    else
    {
      segts.push_back(*std::move(middleEnd));
      middle_end_p = segts.size() - 1;
    }
  }

  segts.push_back(dat_base);
  SegmentData& dat = segts.back();

  // Re-link the segments
  // Create a new segment if we cut in the middle of one
  if(middle_begin_p && middle_end_p
     && segts[*middle_begin_p].id == segts[*middle_end_p].id)
  {
    segts[*middle_end_p].id = getSegmentId(segts);
    for(auto& seg : segts)
    {
      if(seg.id == segts[*middle_end_p].following)
      {
        seg.previous = segts[*middle_end_p].id;
        break;
      }
    }
  }

  if(middle_begin_p)
  {
    SegmentData& seg = segts[*middle_begin_p];
    seg.end = m_minPress;
    seg.following = dat.id;
    dat.previous = seg.id;
    if(seg.previous)
    {
      for(auto& other : segts)
      {
        if(other.id == seg.previous)
        {
          other.following = seg.id;
          break;
        }
      }
    }
  }

  if(middle_end_p)
  {
    SegmentData& seg = segts[*middle_end_p];
    seg.start = m_maxPress;
    seg.previous = dat.id;
    dat.following = seg.id;
    if(seg.following)
    {
      for(auto& other : segts)
      {
        if(other.id == seg.following)
        {
          other.previous = seg.id;
          break;
        }
      }
    }
  }

  checkValidity(segts);
  submit(std::move(segts));
}

void PenCommandObject::release()
{
  auto segts_tpl = filterSegments();
  // First handle the case of a single point
  if(m_segment.points().size() <= 1
     || (std::abs(m_maxPress.x() - m_minPress.x()) < 1e-8))
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

void PenCommandObject::release_n(FilteredSegments&& segts_tpl)
{
  auto& set = score::AppContext().settings<Curve::Settings::Model>();
  m_segment.simplify(std::max(set.getSimplificationRatio(), 100));
  auto lin_segments = m_segment.toPowerSegments();

  auto& segts = segts_tpl.segments;
  checkValidity(segts);

  segts.reserve(segts.size() + lin_segments.size() + 3);

  std::optional<std::size_t> middle_begin_p{};
  std::optional<std::size_t> middle_end_p{};

  if(auto& middleBegin = segts_tpl.middleBegin)
  {
    if(middleBegin->start.x() >= m_minPress.x())
    {
      lin_segments.front().previous = middleBegin->previous;
      if(lin_segments.front().previous)
      {
        for(auto& other : segts)
        {
          if(other.id == lin_segments.front().previous)
          {
            other.following = lin_segments.front().id;
            other.end = lin_segments.front().start;
            break;
          }
        }
      }
      middleBegin = std::nullopt;
    }
    else
    {
      segts.push_back(*std::move(middleBegin));
      middle_begin_p = segts.size() - 1;
    }
  }

  if(auto& middleEnd = segts_tpl.middleEnd)
  {
    if(middleEnd->end.x() <= m_maxPress.x())
    {
      lin_segments.back().following = middleEnd->following;
      if(lin_segments.back().following)
      {
        for(auto& other : segts)
        {
          if(other.id == lin_segments.back().following)
          {
            other.previous = lin_segments.back().id;
            other.start = lin_segments.back().end;
            break;
          }
        }
      }
      middleEnd = std::nullopt;
    }
    else
    {
      segts.push_back(*std::move(middleEnd));
      middle_end_p = segts.size() - 1;
    }
  }

  const std::size_t first_inserted_lin = segts.size();
  const std::size_t N = lin_segments.size();

  { // Put the first one
    SegmentData& lin = lin_segments[0];
    lin.id = getSegmentId(segts);
    if(lin_segments.size() > 1)
      lin_segments[1].previous = lin.id;
    segts.push_back(std::move(lin));
  }

  if(N > 1)
  {
    // Main loop
    for(std::size_t i = 1; i < N - 1; i++)
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
      lin.previous = segts.back().id;
      segts.push_back(std::move(lin));
    }
  }

  // Handle the case of the whole drawn curve being
  // contained in a single original segment
  if(middle_begin_p && middle_end_p
     && segts[*middle_begin_p].id == segts[*middle_end_p].id)
  {
    auto& mb = segts[*middle_begin_p];
    auto& me = segts[*middle_end_p];
    me.id = getSegmentId(segts);
    for(auto& seg : segts)
    {
      if(seg.id == mb.following)
      {
        seg.previous = me.id;
        break;
      }
    }
  }

  // Link the new segments
  auto& first_lin = segts[first_inserted_lin];
  if(middle_begin_p)
  {
    SegmentData& seg = segts[*middle_begin_p];
    seg.end = first_lin.start;
    seg.following = first_lin.id;
    first_lin.previous = seg.id;
    if(seg.previous)
    {
      for(auto& other : segts)
      {
        if(other.id == seg.previous)
        {
          other.following = seg.id;
          break;
        }
      }
    }
  }

  auto& last_lin = segts.back();
  if(middle_end_p)
  {
    SegmentData& seg = segts[*middle_end_p];
    seg.start = last_lin.end;
    seg.previous = last_lin.id;
    last_lin.following = seg.id;
    if(seg.following)
    {
      for(auto& other : segts)
      {
        if(other.id == seg.following)
        {
          other.previous = seg.id;
          break;
        }
      }
    }
  }

  checkValidity(segts);
  submit(std::move(segts));
  m_dispatcher.commit();
  m_segment.reset();
}

PenCommandObject::FilteredSegments PenCommandObject::filterSegments()
{
  PenCommandObject::FilteredSegments ret;
  auto x = m_state->currentPoint.x();
  if(x < m_minPress.x())
    m_minPress = m_state->currentPoint;
  if(x > m_maxPress.x())
    m_maxPress = m_state->currentPoint;

  m_segment.setStart(m_minPress);
  m_segment.setEnd(m_maxPress);
  m_segment.setMinX(m_minPress.x());
  m_segment.setMaxX(m_maxPress.x());
  m_segment.addPointUnscaled(x, m_state->currentPoint.y());

  ret.segments = m_startSegments;
  auto& segts = ret.segments;

  checkValidity(segts);

  // remove all segments that start after minPress and end after maxPress
  std::optional<SegmentData>& middleBegin = ret.middleBegin;
  std::optional<SegmentData>& middleEnd = ret.middleEnd;
  std::vector<std::vector<SegmentData>::iterator> its_to_delete;
  its_to_delete.reserve(segts.size());
  for(auto it = segts.begin(); it != segts.end(); ++it)
  {
    const SegmentData& segt = *it;
    auto start_x = segt.start.x();
    auto end_x = segt.end.x();
    bool to_delete = false;
    if(start_x >= m_minPress.x() && end_x <= m_maxPress.x())
    {
      to_delete = true;
    }
    else
    {
      if(start_x < m_minPress.x() && end_x >= m_minPress.x())
      {
        ret.id_before_middleBegin = segt.previous;
        middleBegin = segt;
        to_delete = true;
      }
      if(start_x <= m_maxPress.x() && end_x > m_maxPress.x())
      {
        ret.id_after_middleEnd = segt.following;
        middleEnd = segt;
        to_delete = true;
      }
    }

    if(to_delete)
    {
      its_to_delete.push_back(it);
    }
  }

  for(auto rit = its_to_delete.rbegin(); rit != its_to_delete.rend(); ++rit)
  {
    auto it = *rit;

    for(auto& seg : segts)
    {
      if(seg.following == it->id)
        seg.following = std::nullopt;
      if(seg.previous == it->id)
        seg.previous = std::nullopt;
    }
    segts.erase(it);
  }
  checkValidity(segts);

  return ret;
}
}
