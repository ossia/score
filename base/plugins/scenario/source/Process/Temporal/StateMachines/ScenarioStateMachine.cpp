#include "ScenarioStateMachine.hpp"
#include "Tools/CreationToolState.hpp"
#include "Tools/MoveToolState.hpp"
#include "Tools/Selection/SelectionToolState.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <Process/ScenarioModel.hpp>

#include "StateMachineCommon.hpp"
#include <QKeyEventTransition>
#include <QSignalTransition>

ScenarioStateMachine::ScenarioStateMachine(TemporalScenarioPresenter& presenter):
    m_presenter{presenter},
    m_commandStack{
        iscore::IDocument::documentFromObject(
            m_presenter.m_viewModel->sharedProcessModel())->commandStack()},
    m_locker{iscore::IDocument::documentFromObject(m_presenter.m_viewModel->sharedProcessModel())->locker()}
{
    // TODO maybe we should directly map
    // the mouseEvent coming from the view,
    // with a QMouseEventTransition (or a derived member) ?
    auto QPointFToScenarioPoint = [&] (const QPointF& point) -> ScenarioPoint
    {
        // TODO : why isn't this mapped to scene pos, and why does it work ?
        return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()),
                point.y() /  m_presenter.view().boundingRect().height()};
    };

    connect(m_presenter.m_view,       &TemporalScenarioView::scenarioPressed,
            [=] (const QPointF& point)
    {
        scenePoint = m_presenter.m_view->mapToScene(point);
        scenarioPoint = QPointFToScenarioPoint(point);
        this->postEvent(new ScenarioPress_Event);
    });
    connect(m_presenter.m_view,       &TemporalScenarioView::scenarioReleased,
            [=] (const QPointF& point)
    {
        scenePoint = m_presenter.m_view->mapToScene(point);
        scenarioPoint = QPointFToScenarioPoint(point);
        this->postEvent(new ScenarioRelease_Event);
    });
    connect(m_presenter.m_view,       &TemporalScenarioView::scenarioMoved,
            [=] (const QPointF& point)
    {
        scenePoint = m_presenter.m_view->mapToScene(point);
        scenarioPoint = QPointFToScenarioPoint(point);
        this->postEvent(new ScenarioMove_Event);
    });


    auto createState = new CreationToolState{*this};
    this->addState(createState);

    auto moveState = new MoveToolState{*this};
    this->addState(moveState);

    auto selectState = new SelectionToolState{*this};
    this->addState(selectState);

    // TODO Toolbar instead
    /*
    auto trans1 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyPress, Qt::Key_M, createState);
    trans1->setTargetState(moveState);
    auto trans2 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyRelease, Qt::Key_M, moveState);
    trans2->setTargetState(createState);

    auto trans3 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyPress, Qt::Key_S, createState);
    trans3->setTargetState(selectState);
    auto trans4 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyRelease, Qt::Key_S, selectState);
    trans4->setTargetState(createState);
    */
    auto t_move_create = new QSignalTransition(this, SIGNAL(setCreateState()), moveState);
    t_move_create->setTargetState(createState);
    auto t_move_select = new QSignalTransition(this, SIGNAL(setSelectState()), moveState);
    t_move_select->setTargetState(selectState);

    auto t_select_create = new QSignalTransition(this, SIGNAL(setCreateState()), selectState);
    t_select_create->setTargetState(createState);
    auto t_select_move = new QSignalTransition(this, SIGNAL(setMoveState()), selectState);
    t_select_move->setTargetState(moveState);

    auto t_create_move = new QSignalTransition(this, SIGNAL(setMoveState()), createState);
    t_create_move->setTargetState(moveState);
    auto t_create_select = new QSignalTransition(this, SIGNAL(setSelectState()), createState);
    t_create_select->setTargetState(selectState);

    createState->start();
    moveState->start();
    selectState->start();

    this->setInitialState(selectState);
}

const ScenarioModel& ScenarioStateMachine::model() const
{ return static_cast<ScenarioModel&>(*m_presenter.m_viewModel->sharedProcessModel()); }
