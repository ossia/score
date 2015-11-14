#include "ScenarioSelectionState.hpp"

#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>

#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <QGraphicsScene>
namespace Scenario
{
SelectionState::SelectionState(
        iscore::SelectionStack& stack,
        const Scenario::ToolPalette& parentSM,
        TemporalScenarioView& scenarioview,
        QState* parent):
    CommonSelectionState{stack, &scenarioview, parent},
    m_parentSM{parentSM},
    m_scenarioView{scenarioview}
{
}


const QPointF& SelectionState::initialPoint() const
{ return m_initialPoint; }


const QPointF& SelectionState::movePoint() const
{ return m_movePoint; }


void SelectionState::on_pressAreaSelection()
{
    m_initialPoint = m_parentSM.scenePoint;
}


void SelectionState::on_moveAreaSelection()
{
    m_movePoint = m_parentSM.scenePoint;
    auto area = QRectF{m_scenarioView.mapFromScene(m_initialPoint),
            m_scenarioView.mapFromScene(m_movePoint)}.normalized();
    m_scenarioView.setSelectionArea(area);
    setSelectionArea(area);
}


void SelectionState::on_releaseAreaSelection()
{
    if(m_parentSM.scenePoint == m_initialPoint)
        on_deselect();

    m_scenarioView.setSelectionArea(QRectF{});
}


void SelectionState::on_deselect()
{
    dispatcher.setAndCommit(Selection{});
    m_scenarioView.setSelectionArea(QRectF{});
}


void SelectionState::on_delete()
{
    ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
    mgr.removeSelection(m_parentSM.model());
}


void SelectionState::on_deleteContent()
{
    ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
    mgr.clearContentFromSelection(m_parentSM.model());
}


void SelectionState::setSelectionArea(const QRectF& area)
{
    using namespace std;
    Selection sel;

    for(const auto& elt : m_parentSM.presenter().constraints())
    {
        if(area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
        {
            sel.append(&elt.model());
        }
    }
    for(const auto& elt : m_parentSM.presenter().timeNodes())
    {
        if(area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
        {
            sel.append(&elt.model());
        }
    }
    for(const auto& elt : m_parentSM.presenter().events())
    {
        if(area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
        {
            sel.append(&elt.model());
        }
    }
    for(const auto& elt : m_parentSM.presenter().states())
    {
        if(area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
        {
            sel.append(&elt.model());
        }
    }

    dispatcher.setAndCommit(filterSelections(sel,
                                             m_parentSM.model().selectedChildren(),
                                             multiSelection()));
}
}
