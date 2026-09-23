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
#include <score/application/ApplicationContext.hpp>

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
  // Stable over the stroke: each move then updates these in place.
  SegmentIdAllocator ids{m_startSegments};
  m_segment.setId(ids.next());
  m_splitId = ids.next();
  m_segment.setMinY(0);
  m_segment.setMaxY(1);
  m_originalPress = m_state->currentPoint;
  m_minPress = m_originalPress;
  m_maxPress = m_originalPress;
}

void PenCommandObject::move()
{
  const auto point_array_id = m_segment.id();
  auto& segts_tpl = filterSegments();
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
  // middleBegin starts before the stroke, middleEnd ends after it.
  if(middleBegin)
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = segts.size() - 1;
  }
  if(middleEnd)
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = segts.size() - 1;
  }

  segts.push_back(dat_base);
  SegmentData& dat = segts.back();

  // Re-link the segments
  // Create a new segment if we cut in the middle of one
  if(middle_begin_p && middle_end_p
     && segts[*middle_begin_p].id == segts[*middle_end_p].id)
  {
    segts[*middle_end_p].id = m_splitId;
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
    setSegmentExtent(seg, seg.start, m_minPress);
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
    setSegmentExtent(seg, m_maxPress, seg.end);
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
  submit(segts);
}

void PenCommandObject::release()
{
  auto& segts_tpl = filterSegments();
  // First handle the case of a single point
  if(m_segment.points().size() <= 1
     || (std::abs(m_maxPress.x() - m_minPress.x()) < 1e-8))
  {
    cancel();
  }
  else
  {
    release_n(segts_tpl);
  }
}

void PenCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_segment.reset();
}

void PenCommandObject::release_n(FilteredSegments& segts_tpl)
{
  auto& set = score::AppContext().settings<Curve::Settings::Model>();
  m_segment.simplify(std::max(set.getSimplificationRatio(), 100));
  auto lin_segments = m_segment.toPowerSegments();

  auto& segts = segts_tpl.segments;
  checkValidity(segts);

  segts.reserve(segts.size() + lin_segments.size() + 3);

  std::optional<std::size_t> middle_begin_p{};
  std::optional<std::size_t> middle_end_p{};

  // middleBegin starts before the stroke, middleEnd ends after it.
  if(auto& middleBegin = segts_tpl.middleBegin)
  {
    segts.push_back(*std::move(middleBegin));
    middle_begin_p = segts.size() - 1;
  }

  if(auto& middleEnd = segts_tpl.middleEnd)
  {
    segts.push_back(*std::move(middleEnd));
    middle_end_p = segts.size() - 1;
  }

  const std::size_t first_inserted_lin = segts.size();
  const std::size_t N = lin_segments.size();
  SegmentIdAllocator ids{segts};

  { // Put the first one
    SegmentData& lin = lin_segments[0];
    lin.id = ids.next();
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
      lin.id = ids.next();

      segts[first_inserted_lin + i - 1].following = lin.id;

      lin_segments[i + 1].previous = lin.id;

      segts.push_back(std::move(lin));
    }

    { // Put the last one
      SegmentData& lin = lin_segments.back();
      lin.id = ids.next();
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
    me.id = ids.next();
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
    setSegmentExtent(seg, seg.start, first_lin.start);
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
    setSegmentExtent(seg, last_lin.end, seg.end);
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
  submit(segts);
  m_dispatcher.commit();
  m_segment.reset();
}

PenCommandObject::FilteredSegments& PenCommandObject::filterSegments()
{
  auto& ret = m_filtered;
  ret.id_before_middleBegin = std::nullopt;
  ret.id_after_middleEnd = std::nullopt;
  ret.middleBegin = std::nullopt;
  ret.middleEnd = std::nullopt;

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

  // Every segment under the stroke goes; the ones it starts or ends inside of
  // are kept aside, to be cut by the caller.
  auto& segts = ret.segments;
  segts.clear();
  segts.reserve(m_startSegments.size() + 3);
  m_filteredIds.clear();
  for(const SegmentData& segt : m_startSegments)
  {
    const auto start_x = segt.start.x();
    const auto end_x = segt.end.x();
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
        ret.middleBegin = segt;
        to_delete = true;
      }
      if(start_x <= m_maxPress.x() && end_x > m_maxPress.x())
      {
        ret.id_after_middleEnd = segt.following;
        ret.middleEnd = segt;
        to_delete = true;
      }
    }

    if(to_delete)
      m_filteredIds.insert(segt.id.val());
    else
      segts.push_back(segt);
  }

  for(auto& seg : segts)
  {
    if(seg.following && m_filteredIds.contains(seg.following->val()))
      seg.following = std::nullopt;
    if(seg.previous && m_filteredIds.contains(seg.previous->val()))
      seg.previous = std::nullopt;
  }
  checkValidity(segts);

  return ret;
}
}
