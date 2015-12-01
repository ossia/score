#include <QApplication>
#include <QRect>

#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Palette/Tools/MoveTool.hpp>
#include <Curve/Palette/Tools/SmartTool.hpp>
#include "CurvePalette.hpp"
#include <Process/ProcessContext.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>

namespace iscore {
class CommandStackFacade;
class ObjectLocker;
}  // namespace iscore

namespace Curve
{

ToolPalette::ToolPalette(
        LayerContext& f,
        CurvePresenter& pres):
    GraphicsSceneToolPalette{*pres.view().scene()},
    m_presenter{pres},
    m_context{f},
    m_stack{f.commandStack},
    m_locker{f.objectLocker},
    m_selectTool{*this},
    m_createTool{*this},
    m_setSegmentTool{*this},
    m_inputDisp{pres.view(), *this, f}
{
}
CurvePresenter& ToolPalette::presenter() const
{
    return m_presenter;
}

Curve::EditionSettings& ToolPalette::editionSettings() const
{
    return m_presenter.editionSettings();
}

const CurveModel& ToolPalette::model() const
{
    return m_presenter.model();
}

iscore::CommandStackFacade& ToolPalette::commandStack() const
{
    return m_stack;
}

iscore::ObjectLocker& ToolPalette::locker() const
{
    return m_locker;
}

void ToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
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
        default:
            break;
    }
}

void ToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
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
        default:
            break;
    }
}

void ToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
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
        default:
            break;
    }
}

void ToolPalette::on_cancel()
{
    m_createTool.on_cancel();
    m_selectTool.on_cancel();
    m_setSegmentTool.on_cancel();
}

void ToolPalette::activate(Tool)
{

}

void ToolPalette::desactivate(Tool)
{

}

Point ToolPalette::ScenePointToCurvePoint(const QPointF& point)
{
    return {point.x() / m_presenter.view().boundingRect().width(),
            1. - point.y() / m_presenter.view().boundingRect().height()};
}
}
