#include "CurveStateMachine.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
#include <iscore/statemachine/StateMachineTools.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <core/document/Document.hpp>
#include <QSignalTransition>
#include <QActionGroup>

namespace Curve
{
ToolPalette::ToolPalette(
        CurvePresenter& pres,
        QObject* parent):
    GraphicsSceneToolPalette{*pres.view().scene()},
    m_presenter{pres},
    m_stack{iscore::IDocument::commandStack(m_presenter.model())},
    m_locker{iscore::IDocument::locker(m_presenter.model())},
    m_selectTool{*this},
    m_createTool{*this},
    m_setSegmentTool{*this}
{
}

CurvePresenter& ToolPalette::presenter() const
{
    return m_presenter;
}

const CurveModel& ToolPalette::model() const
{
    return m_presenter.model();
}

iscore::CommandStack& ToolPalette::commandStack() const
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
    switch(m_presenter.editionSettings().tool())
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
    switch(m_presenter.editionSettings().tool())
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
    switch(m_presenter.editionSettings().tool())
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
    switch(m_presenter.editionSettings().tool())
    {
        case Curve::Tool::Create:
            m_createTool.on_cancel();
            break;
        case Curve::Tool::Select:
            m_selectTool.on_cancel();
            break;
        case Curve::Tool::SetSegment:
            m_setSegmentTool.on_cancel();
            break;
        default:
            break;
    }
}

Point ToolPalette::ScenePointToCurvePoint(const QPointF& point)
{
    return {point.x() / m_presenter.view().boundingRect().width(),
                1. - point.y() / m_presenter.view().boundingRect().height()};
}
}
