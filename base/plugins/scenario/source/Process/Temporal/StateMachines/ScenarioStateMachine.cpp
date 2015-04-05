#include "ScenarioStateMachine.hpp"
#include "CreationToolState.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <Process/ScenarioModel.hpp>

#include "StateMachineCommon.hpp"

ScenarioStateMachine::ScenarioStateMachine(TemporalScenarioPresenter& presenter):
    m_presenter{presenter},
    m_commandStack{
        iscore::IDocument::documentFromObject(
            m_presenter.m_viewModel->sharedProcessModel())->commandStack()}
{
    auto QPointFToScenarioPoint = [&] (const QPointF& point) -> ScenarioPoint
    {
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
    this->setInitialState(createState);
}

const ScenarioModel& ScenarioStateMachine::model() const
{ return static_cast<ScenarioModel&>(*m_presenter.m_viewModel->sharedProcessModel()); }
