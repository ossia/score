#include "CurveStateMachine.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
#include "Curve/StateMachine/States/Tools/MoveTool.hpp"
#include "Curve/StateMachine/States/Tools/SelectionTool.hpp"
#include "Curve/StateMachine/States/Tools/CreationTool.hpp"
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

    auto edit_act = new QAction(this);
    edit_act->setShortcut(QKeySequence("Alt+g"));
    edit_act->setEnabled(true);
    edit_act->setCheckable(true);
    edit_act->setShortcutContext(Qt::ApplicationShortcut);
    connect(edit_act, &QAction::triggered, this, [&] (bool b)
    {
        changeTool(1);
    });

    g->addAction(sel_act);
    g->addAction(edit_act);

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
    m_transitionState = new QState{this};
    m_selectTool = new Curve::SelectionTool(*this);
    m_editTool = new Curve::EditionTool(*this);

    this->setInitialState(m_selectTool);

    auto t_exit_select = new QSignalTransition(this, SIGNAL(exitState()), m_selectTool);
    t_exit_select->setTargetState(m_transitionState);
    auto t_exit_edit = new QSignalTransition(this, SIGNAL(exitState()), m_editTool);
    t_exit_edit->setTargetState(m_transitionState);

    auto t_enter_select = new QSignalTransition(this, SIGNAL(setSelectionState()), m_transitionState);
    t_enter_select->setTargetState(m_selectTool);
    auto t_enter_edit= new QSignalTransition(this, SIGNAL(setEditionState()), m_transitionState);
    t_enter_edit->setTargetState(m_editTool);
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
        case static_cast<int>(Curve::Tool::Selection):
            emit setSelectionState();
            break;
        case static_cast<int>(Curve::Tool::Edition):
            emit setEditionState();
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
            Q_ASSERT(false);
            break;
    }
}

int CurveStateMachine::tool() const
{
    if(m_editTool->active())
        return (int)Curve::Tool::Edition;
    if(m_selectTool->active())
        return (int)Curve::Tool::Selection;

    return (int)Curve::Tool::Selection;
}

