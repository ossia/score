#include "CurveStateMachine.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
#include "Curve/StateMachine/States/Tools/MoveTool.hpp"
#include "Curve/StateMachine/States/Tools/SelectionTool.hpp"
#include <iscore/statemachine/StateMachineTools.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <core/document/Document.hpp>
#include <QSignalTransition>
#include <QActionGroup>

CurveStateMachine::CurveStateMachine(
        CurvePresenter& pres,
        QObject* parent):
    BaseStateMachine{*pres.view().scene()},
    m_presenter{pres},
    m_stack{iscore::IDocument::commandStack(m_presenter.model())},
    m_locker{iscore::IDocument::locker(m_presenter.model())}
{
    setupPostEvents();
    setupStates();

    start();
}

CurvePresenter& CurveStateMachine::presenter() const
{
    return m_presenter;
}

const CurveModel& CurveStateMachine::model() const
{
    return m_presenter.model();
}

iscore::CommandStack& CurveStateMachine::commandStack() const
{
    return m_stack;
}

iscore::ObjectLocker& CurveStateMachine::locker() const
{
    return m_locker;
}

void CurveStateMachine::setupStates()
{
    m_transitionState = new QState{this};
    m_selectTool = new Curve::SelectionAndMoveTool(*this);

    m_createTool = new Curve::CreateTool(*this);
    m_moveTool = new Curve::MoveTool(*this);
    m_setSegmentTool = new Curve::SetSegmentTool(*this);

    this->setInitialState(m_moveTool);

    auto t_exit_select = new QSignalTransition(this, SIGNAL(exitState()), m_selectTool);
    t_exit_select->setTargetState(m_transitionState);
    auto t_exit_create = new QSignalTransition(this, SIGNAL(exitState()), m_createTool);
    t_exit_create->setTargetState(m_transitionState);
    auto t_exit_move = new QSignalTransition(this, SIGNAL(exitState()), m_moveTool);
    t_exit_move->setTargetState(m_transitionState);
    auto t_exit_setsegment = new QSignalTransition(this, SIGNAL(exitState()), m_setSegmentTool);
    t_exit_setsegment->setTargetState(m_transitionState);

    auto t_enter_select = new QSignalTransition(this, SIGNAL(setSelectionState()), m_transitionState);
    t_enter_select->setTargetState(m_selectTool);
    auto t_enter_create= new QSignalTransition(this, SIGNAL(setCreateState()), m_transitionState);
    t_enter_create->setTargetState(m_createTool);
    auto t_enter_move= new QSignalTransition(this, SIGNAL(setMoveState()), m_transitionState);
    t_enter_move->setTargetState(m_moveTool);
    auto t_enter_setSegment = new QSignalTransition(this, SIGNAL(setSetSegmentState()), m_transitionState);
    t_enter_setSegment->setTargetState(m_setSegmentTool);
}

void CurveStateMachine::setupPostEvents()
{
    auto QPointFToCurvePoint = [&] (const QPointF& point) -> CurvePoint
    {
        return {point.x() / m_presenter.view().boundingRect().width(),
                    1. - point.y() / m_presenter.view().boundingRect().height()};
    };

    auto updateData = [=] (const QPointF& point)
    {
        scenePoint = point;
        curvePoint = QPointFToCurvePoint(m_presenter.view().mapFromScene(point));
    };

    con(m_presenter.view(), &CurveView::pressed,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Press_Event);
    });

    con(m_presenter.view(), &CurveView::moved,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Move_Event);
    });

    con(m_presenter.view(), &CurveView::released,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Release_Event);
    });

    // TODO generalize this.
    con(m_presenter.view(), &CurveView::escPressed,
            this, [&] ()
    {
        this->postEvent(new Cancel_Event);
    });
}

void CurveStateMachine::changeTool(int state)
{
    emit exitState();
    switch(state)
    {
        case static_cast<int>(Curve::Tool::Selection):
            emit setSelectionState();
            break;
        case static_cast<int>(Curve::Tool::Create):
            emit setCreateState();
            break;
        case static_cast<int>(Curve::Tool::Move):
            emit setMoveState();
            break;
        case static_cast<int>(Curve::Tool::SetSegment):
            emit setSetSegmentState();
            break;
            /*
        case static_cast<int>(Curve::Tool::CreatePen):
            emit ();
            break;
        case static_cast<int>(Curve::Tool::RemovePen):
            emit ();
            break;
            */
        default:
            ISCORE_ABORT;
            break;
    }
}

int CurveStateMachine::tool() const
{
    if(isStateActive(m_createTool))
        return (int)Curve::Tool::Create;
    if(isStateActive(m_moveTool))
        return (int)Curve::Tool::Create;
    if(isStateActive(m_setSegmentTool))
        return (int)Curve::Tool::Create;
    if(isStateActive(m_selectTool))
        return (int)Curve::Tool::Selection;

    return (int)Curve::Tool::Selection;
}

