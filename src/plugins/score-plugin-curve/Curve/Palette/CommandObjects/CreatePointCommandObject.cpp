// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreatePointCommandObject.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>

#include <QVariant>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
CreatePointCommandObject::CreatePointCommandObject(
    Presenter* presenter,
    const score::CommandStackFacade& stack)
    : CommandObjectBase{presenter, stack}
{
}

CreatePointCommandObject::~CreatePointCommandObject() { }

void CreatePointCommandObject::on_press()
{
  // Save the start data.
  m_originalPress = m_state->currentPoint;

  for (PointModel* pt : m_presenter->model().points())
  {
    auto pt_x = pt->pos().x();

    if (pt_x >= m_xmin && pt_x < m_originalPress.x())
    {
      m_xmin = pt_x;
    }
    if (pt_x <= m_xmax && pt_x > m_originalPress.x())
    {
      m_xmax = pt_x;
    }
    if (pt_x >= m_xLastPoint)
    {
      m_xLastPoint = pt_x;
    }
  }

  m_xLastPoint = m_xmax;

  move();
}

void CreatePointCommandObject::move()
{
  auto segments = m_startSegments;

  // Locking between bounds
  handleLocking();

  // Creation
  createPoint(segments);

  // Submit
  submit(std::move(segments));
}

void CreatePointCommandObject::release()
{
  m_dispatcher.commit();
}

void CreatePointCommandObject::cancel()
{
  m_dispatcher.rollback();
}

void CreatePointCommandObject::createPoint(std::vector<SegmentData>& segments)
{
  // Create a point where we clicked
  // By creating segments that goes to the closest points if we're in the
  // empty,
  // or by splitting a segment if we're in the middle of it.
  // 1. Check if we're clicking in a place where there is a segment
  SegmentData* middle = nullptr;
  SegmentData* exactBefore = nullptr;
  SegmentData* exactAfter = nullptr;
  const auto current_x = m_state->currentPoint.x();
  for (auto& segment : segments)
  {
    if (segment.start.x() < current_x && current_x < segment.end.x())
      middle = &segment;
    if (segment.end.x() == current_x)
      exactBefore = &segment;
    if (segment.start.x() == current_x)
      exactAfter = &segment;

    if (middle && exactBefore && exactAfter)
      break;
  }

  // Handle creation on an exact other point
  if (exactBefore || exactAfter)
  {
    if (exactBefore)
    {
      exactBefore->end = m_state->currentPoint;
    }
    if (exactAfter)
    {
      exactAfter->start = m_state->currentPoint;
    }
  }
  else if (middle)
  {
    // TODO refactor with MovePointState (line ~330)
    // The segment goes in the first half of "middle"
    SegmentData newSegment{
        getSegmentId(segments),
        middle->start,
        m_state->currentPoint,
        middle->previous,
        middle->id,
        middle->type,
        middle->specificSegmentData};

    auto prev_it = find(segments, middle->previous);
    // TODO we shouldn't have to test for this, only test if middle->previous
    // != id{}
    if (prev_it != segments.end())
    {
      (*prev_it).following = newSegment.id;
    }

    middle->start = m_state->currentPoint;
    middle->previous = newSegment.id;
    segments.push_back(newSegment);
  }
  else
  {
    // ~ Creating in the void ~ ... Spooky!

    // This is of *utmost* importance : if we don't do this,
    // when we push_back, the pointers get invalidated because the memory
    // has been moved, which causes a wealth of uncanny bugs and random memory
    // errors
    // in other threads. So we reserve from up front the size we'll need.
    segments.reserve(segments.size() + 2);

    double seg_closest_from_left_x = 0;
    SegmentData* seg_closest_from_left{};
    double seg_closest_from_right_x = 1.;
    SegmentData* seg_closest_from_right{};
    for (SegmentData& segment : segments)
    {
      auto seg_start_x = segment.start.x();
      if (seg_start_x > current_x && seg_start_x < seg_closest_from_right_x)
      {
        seg_closest_from_right_x = seg_start_x;
        seg_closest_from_right = &segment;
      }

      auto seg_end_x = segment.end.x();
      if (seg_end_x < current_x && seg_end_x > seg_closest_from_left_x)
      {
        seg_closest_from_left_x = seg_end_x;
        seg_closest_from_left = &segment;
      }
    }

    // Create a curve segment for the left
    // We do this little dance because of id generation.
    {
      SegmentData newLeftSegment;
      newLeftSegment.id = getSegmentId(segments);
      segments.push_back(newLeftSegment);
    }
    SegmentData& newLeftSegment = segments.back();
    newLeftSegment.type = Metadata<ConcreteKey_k, PowerSegment>::get();
    newLeftSegment.specificSegmentData = QVariant::fromValue(PowerSegmentData{PowerSegmentData::linearGamma});
    newLeftSegment.start = {seg_closest_from_left_x, 0.};
    newLeftSegment.end = m_state->currentPoint;

    if (seg_closest_from_left)
    {
      newLeftSegment.start = seg_closest_from_left->end;
      newLeftSegment.previous = seg_closest_from_left->id;

      seg_closest_from_left->following = newLeftSegment.id;
    }

    // Create a curve segment for the right
    // If we are before 1.0 we wrap to 1.0.
    if (current_x <= 1.0 || seg_closest_from_right)
    {
      {
        SegmentData newRightSegment;
        newRightSegment.id = getSegmentId(segments);
        segments.push_back(newRightSegment);
      }
      SegmentData& newRightSegment = segments.back();
      newRightSegment.type = Metadata<ConcreteKey_k, PowerSegment>::get();
      newRightSegment.specificSegmentData = QVariant::fromValue(PowerSegmentData{PowerSegmentData::linearGamma});
      newRightSegment.start = m_state->currentPoint;
      newRightSegment.end = {seg_closest_from_right_x, 0.};

      newLeftSegment.following = newRightSegment.id;
      newRightSegment.previous = newLeftSegment.id;

      if (seg_closest_from_right)
      {
        newRightSegment.end = seg_closest_from_right->start;
        newRightSegment.following = seg_closest_from_right->id;

        seg_closest_from_right->previous = newRightSegment.id;
      }
    }
  }
}
}
