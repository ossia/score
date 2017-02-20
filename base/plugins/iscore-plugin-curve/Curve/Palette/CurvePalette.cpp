#include "CurvePalette.hpp"

namespace Curve
{

ToolPalette::ToolPalette(const iscore::DocumentContext& ctx, Presenter& pres)
    : GraphicsSceneToolPalette{pres.view()}
    , m_presenter{pres}
    , m_selectTool{*this, ctx}
    , m_createTool{*this, ctx}
    , m_setSegmentTool{*this, ctx}
    , m_createPenTool{*this, ctx}
{
}

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
  scenePoint = point;
  auto curvePoint
      = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  switch (editionSettings().tool())
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
  auto curvePoint
      = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  switch (editionSettings().tool())
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
  scenePoint = point;
  auto curvePoint
      = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
  switch (editionSettings().tool())
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
  m_createTool.on_cancel();
  m_selectTool.on_cancel();
  m_setSegmentTool.on_cancel();
  m_createPenTool.on_cancel();
}

void ToolPalette::activate(Curve::Tool)
{
}
void ToolPalette::desactivate(Curve::Tool)
{
}

void ToolPalette::createPoint(QPointF point)
{
  scenePoint = point;
  auto curvePoint
      = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));

  m_createTool.on_pressed(point, curvePoint);
  m_createTool.on_released(point, curvePoint);
}
}
