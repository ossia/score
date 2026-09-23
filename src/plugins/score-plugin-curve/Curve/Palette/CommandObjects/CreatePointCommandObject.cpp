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

#include <ossia/math/safe_math.hpp>

#include <QVariant>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
CreatePointCommandObject::CreatePointCommandObject(
    const Model& model, Presenter* presenter, const score::CommandStackFacade& stack)
    : CommandObjectBase{model, presenter, stack}
{
}

CreatePointCommandObject::~CreatePointCommandObject() { }

void CreatePointCommandObject::on_press()
{
  // Save the start data.
  m_originalPress = m_state->currentPoint;

  for(PointModel* pt : m_model.points())
  {
    auto pt_x = pt->pos().x();

    if(pt_x >= m_xmin && pt_x < m_originalPress.x())
    {
      m_xmin = pt_x;
    }
    if(pt_x <= m_xmax && pt_x > m_originalPress.x())
    {
      m_xmax = pt_x;
    }
    if(pt_x >= m_xLastPoint)
    {
      m_xLastPoint = pt_x;
    }
  }

  m_xLastPoint = m_xmax;

  move();
}

void CreatePointCommandObject::move()
{
  // Locking between bounds
  handleLocking();
  if(!ossia::safe_isfinite(m_state->currentPoint.x())
     || !ossia::safe_isfinite(m_state->currentPoint.y()))
    return;

  m_segments = m_startSegments;
  createPoint(m_segments);
  submit(m_segments);
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
  createPointAt(segments, m_state->currentPoint);
}
}
