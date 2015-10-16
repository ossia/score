#include "ScenarioSelectionState.hpp"

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include "Document/Event/EventView.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/Constraint/ViewModels/ConstraintView.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"

#include "Process/ScenarioModel.hpp"
#include <QGraphicsScene>
ScenarioSelectionState::ScenarioSelectionState(
        iscore::SelectionStack& stack,
        const ScenarioStateMachine& parentSM,
        TemporalScenarioView& scenarioview,
        QState* parent):
    CommonSelectionState{stack, &scenarioview, parent},
    m_parentSM{parentSM},
    m_scenarioView{scenarioview}
{
}


const QPointF& ScenarioSelectionState::initialPoint() const
{ return m_initialPoint; }


const QPointF& ScenarioSelectionState::movePoint() const
{ return m_movePoint; }


void ScenarioSelectionState::on_pressAreaSelection()
{
    m_initialPoint = m_parentSM.scenePoint;
}


void ScenarioSelectionState::on_moveAreaSelection()
{
    m_movePoint = m_parentSM.scenePoint;
    auto area = QRectF{m_scenarioView.mapFromScene(m_initialPoint),
            m_scenarioView.mapFromScene(m_movePoint)}.normalized();
    m_scenarioView.setSelectionArea(area);
    setSelectionArea(area);
}


void ScenarioSelectionState::on_releaseAreaSelection()
{
    if(m_parentSM.scenePoint == m_initialPoint)
        on_deselect();

    m_scenarioView.setSelectionArea(QRectF{});
}


void ScenarioSelectionState::on_deselect()
{
    dispatcher.setAndCommit(Selection{});
    m_scenarioView.setSelectionArea(QRectF{});
}


void ScenarioSelectionState::on_delete()
{
    ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
    mgr.removeSelection(m_parentSM.model());
}


void ScenarioSelectionState::on_deleteContent()
{
    ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
    mgr.clearContentFromSelection(m_parentSM.model());
}


void ScenarioSelectionState::setSelectionArea(const QRectF& area)
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
