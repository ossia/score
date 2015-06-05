#include "CurveStateMachine.hpp"
#include "CurveTest/CurveModel.hpp"
#include "CurveTest/CurvePresenter.hpp"
#include "CurveTest/CurveView.hpp"
#include "CurveTest/StateMachine/States/Tools/MoveTool.hpp"
#include "CurveTest/StateMachine/States/Tools/SelectionTool.hpp"
#include "CurveTest/StateMachine/States/Tools/CreationTool.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <core/document/Document.hpp>
#include <QSignalTransition>
#include <QActionGroup>

CurveStateMachine::CurveStateMachine(
        CurvePresenter& pres,
        QObject* parent):
    BaseStateMachine{*pres.view().scene()},
    m_presenter{pres},
    m_stack{
        iscore::IDocument::documentFromObject(
            m_presenter.model()) ->commandStack()
        },
    m_locker{
        iscore::IDocument::documentFromObject(
            m_presenter.model()) ->locker()
        }
{
    setupPostEvents();
    setupStates();

    QActionGroup* g = new QActionGroup(this);
    auto sel_act = new QAction(this);
    sel_act->setShortcut(QKeySequence("Alt+f"));
    sel_act->setEnabled(true);
    sel_act->setCheckable(true);
    sel_act->setChecked(true);
    sel_act->setShortcutContext(Qt::ApplicationShortcut);
    connect(sel_act, &QAction::triggered, this, [&] (bool b)
    {
        changeTool(0);
    });

    auto mov_act = new QAction(this);
    mov_act->setShortcut(QKeySequence("Alt+g"));
    mov_act->setEnabled(true);
    mov_act->setCheckable(true);
    mov_act->setShortcutContext(Qt::ApplicationShortcut);
    connect(mov_act, &QAction::triggered, this, [&] (bool b)
    {
        changeTool(1);
    });

    auto cre_act = new QAction(this);
    cre_act->setShortcut(QKeySequence("Alt+h"));
    cre_act->setEnabled(true);
    cre_act->setCheckable(true);
    cre_act->setShortcutContext(Qt::ApplicationShortcut);
    connect(cre_act, &QAction::triggered, this, [&] (bool b)
    {
        changeTool(2);
    });
    g->addAction(mov_act);
    g->addAction(sel_act);
    g->addAction(cre_act);

    start();
}

CurvePresenter& CurveStateMachine::presenter() const
{
    return m_presenter;
}

const CurveModel& CurveStateMachine::model() const
{
    return *m_presenter.model();
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
    //m_transitionState = new QState{this};
    //m_transitionState->setParent(this);

    //m_selectTool = new Curve::SelectionTool(*this);
    m_moveTool = new Curve::MoveTool(*this);
    //m_createTool = new Curve::CreationTool(*this);

    //this->addState(m_transitionState);
    //this->addState(m_selectTool);
    this->addState(m_moveTool);
    //this->addState(m_createTool);
    this->setInitialState(m_moveTool);

    auto t_exit_select = new QSignalTransition(this, SIGNAL(exitState()), m_selectTool);
    t_exit_select->setTargetState(m_transitionState);
    auto t_exit_move = new QSignalTransition(this, SIGNAL(exitState()), m_moveTool);
    t_exit_move->setTargetState(m_transitionState);
    auto t_exit_create = new QSignalTransition(this, SIGNAL(exitState()), m_createTool);
    t_exit_create->setTargetState(m_transitionState);

    auto t_enter_select = new QSignalTransition(this, SIGNAL(setSelectState()), m_transitionState);
    t_enter_select->setTargetState(m_selectTool);
    auto t_enter_move = new QSignalTransition(this, SIGNAL(setMoveState()), m_transitionState);
    t_enter_move->setTargetState(m_moveTool);
    auto t_enter_create= new QSignalTransition(this, SIGNAL(setCreateState()), m_transitionState);
    t_enter_create->setTargetState(m_createTool);
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

    connect(&m_presenter.view(), &CurveView::pressed,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Press_Event);
    });

    connect(&m_presenter.view(), &CurveView::moved,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Move_Event);
    });

    connect(&m_presenter.view(), &CurveView::released,
            this, [=] (const QPointF& point)
    {
        updateData(point);
        postEvent(new Release_Event);
    });

    // TODO generalize this.
    connect(&m_presenter.view(), &CurveView::escPressed,
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
    case static_cast<int>(Curve::Tool::Create):
        emit setCreateState();
        break;
    case static_cast<int>(Curve::Tool::Move):
        emit setMoveState();
        break;
    case static_cast<int>(Curve::Tool::Select):
        emit setSelectState();
        break;

    default:
        Q_ASSERT(false);
        break;
    }
}

