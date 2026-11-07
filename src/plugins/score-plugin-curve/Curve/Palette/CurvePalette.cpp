// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurvePalette.hpp"

#include <score/tools/Debug.hpp>

#include <ossia/math/safe_math.hpp>
namespace Curve
{

ToolPalette::ToolPalette(
    const score::DocumentContext& ctx, Presenter& pres,
    score::SelectionStack* selectionStack)
    : GraphicsSceneToolPalette{*pres.view().scene()}
    , m_presenter{pres}
    , m_selectionStack{selectionStack ? *selectionStack : ctx.selectionStack}
    , m_selectTool{*this, ctx}
    , m_createTool{*this, ctx}
    , m_setSegmentTool{*this, ctx}
    , m_createPenTool{*this, ctx}
{
  SCORE_ASSERT(m_presenter.view().scene());
}

ToolPalette::~ToolPalette() { }

Presenter& ToolPalette::presenter() const
{
  return m_presenter;
}

Curve::EditionSettings& ToolPalette::editionSettings() const
{
  return m_presenter.editionSettings();
}

const Model& ToolPalette::model() const
{
  return m_presenter.model();
}

void ToolPalette::on_pressed(QPointF point)
{
  // A gesture whose release never came is finished where it was left, or the
  // tool's state machine would still be in it and ignore this press.
  if(m_pressed)
    on_released(scenePoint);
  m_pressed = true;

  scenePoint = point;
  auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  if(!ossia::safe_isfinite(curvePoint.x()) || !ossia::safe_isfinite(curvePoint.y()))
    return;
  switch(editionSettings().tool())
  {
    case Curve::Tool::Create:
      m_createTool.on_pressed(point, curvePoint);
      break;
    case Curve::Tool::Select:
      m_selectTool.on_pressed(point, curvePoint);
      break;
    case Curve::Tool::SetSegment:
      m_setSegmentTool.on_pressed(point, curvePoint);
      break;
    case Curve::Tool::CreatePen:
      m_createPenTool.on_pressed(point, curvePoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  if(!ossia::safe_isfinite(curvePoint.x()) || !ossia::safe_isfinite(curvePoint.y()))
    return;
  switch(editionSettings().tool())
  {
    case Curve::Tool::Create:
      m_createTool.on_moved(point, curvePoint);
      break;
    case Curve::Tool::Select:
      m_selectTool.on_moved(point, curvePoint);
      break;
    case Curve::Tool::SetSegment:
      m_setSegmentTool.on_moved(point, curvePoint);
      break;
    case Curve::Tool::CreatePen:
      m_createPenTool.on_moved(point, curvePoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_released(QPointF point)
{
  m_pressed = false;
  scenePoint = point;
  auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  if(!ossia::safe_isfinite(curvePoint.x()) || !ossia::safe_isfinite(curvePoint.y()))
    return;
  switch(editionSettings().tool())
  {
    case Curve::Tool::Create:
      m_createTool.on_released(point, curvePoint);
      break;
    case Curve::Tool::Select:
      m_selectTool.on_released(point, curvePoint);
      break;
    case Curve::Tool::SetSegment:
      m_setSegmentTool.on_released(point, curvePoint);
      break;
    case Curve::Tool::CreatePen:
      m_createPenTool.on_released(point, curvePoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_cancel()
{
  m_pressed = false;
  m_createTool.on_cancel();
  m_selectTool.on_cancel();
  m_setSegmentTool.on_cancel();
  m_createPenTool.on_cancel();
}

void ToolPalette::cancelGesture()
{
  m_pressed = false;
  // The command objects are rolled back directly: an event posted to a state
  // machine that is inside one of its transitions is only handled once that
  // transition returns, which may be after this palette is gone. So is a
  // release still queued: its gesture is rolled back, not committed.
  m_createTool.rollback();
  m_selectTool.rollback();
  m_setSegmentTool.rollback();
  m_createPenTool.rollback();

  // Takes each machine back to its idle state whenever it next runs. The
  // cancelled state rolls back again, which does nothing on a command object
  // that holds no command.
  const auto leave = [](auto& tool) {
    auto& sm = tool.localSM();
    if(sm.isRunning())
      sm.postEvent(new score::Cancel_Event);
  };
  leave(m_createTool);
  leave(m_selectTool);
  leave(m_setSegmentTool);
  leave(m_createPenTool);
}

void ToolPalette::activate(Curve::Tool) { }
void ToolPalette::desactivate(Curve::Tool) { }

void ToolPalette::createPoint(QPointF point)
{
  scenePoint = point;
  auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));

  m_createTool.on_pressed(point, curvePoint);
  m_createTool.on_released(point, curvePoint);
}
}
