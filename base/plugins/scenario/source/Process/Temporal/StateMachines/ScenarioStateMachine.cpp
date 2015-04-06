#include "ScenarioStateMachine.hpp"
#include "Tools/CreationToolState.hpp"
#include "Tools/MoveToolState.hpp"
#include "Tools/SelectionToolState.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <Process/ScenarioModel.hpp>

#include "StateMachineCommon.hpp"
#include <QKeyEventTransition>

ScenarioStateMachine::ScenarioStateMachine(TemporalScenarioPresenter& presenter):
    m_presenter{presenter},
    m_commandStack{
        iscore::IDocument::documentFromObject(
            m_presenter.m_viewModel->sharedProcessModel())->commandStack()},
    m_locker{iscore::IDocument::documentFromObject(m_presenter.m_viewModel->sharedProcessModel())->locker()}
{
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

    auto trans1 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyPress, Qt::Key_M, createState);
    trans1->setTargetState(moveState);
    auto trans2 = new QKeyEventTransition(m_presenter.m_view, QEvent::KeyRelease, Qt::Key_M, moveState);
    trans2->setTargetState(createState);

    createState->start();
    moveState->start();


    this->setInitialState(createState);
}

const ScenarioModel& ScenarioStateMachine::model() const
{ return static_cast<ScenarioModel&>(*m_presenter.m_viewModel->sharedProcessModel()); }
