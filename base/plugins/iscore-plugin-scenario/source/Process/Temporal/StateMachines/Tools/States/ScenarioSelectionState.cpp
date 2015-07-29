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
    m_scenarioView.setSelectionArea(
                QRectF{m_scenarioView.mapFromScene(m_initialPoint),
                       m_scenarioView.mapFromScene(m_movePoint)}.normalized());
    setSelectionArea(QRectF{m_initialPoint, m_movePoint}.normalized());
}


void ScenarioSelectionState::on_releaseAreaSelection()
{
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
    QPainterPath path;
    path.addRect(area);
    Selection sel;

    auto scenario = dynamic_cast<ScenarioInterface*>(&m_parentSM.presenter().layerModel().processModel());
    Q_ASSERT(scenario);
    for(const auto& item : m_parentSM.scene().items(path))
    {
        switch(item->type())
        {
            case QGraphicsItem::UserType + 1: // event
            {
                const auto& ev_model = static_cast<const EventView*>(item)->presenter().model();
                if(ev_model.parentScenario() == scenario)
                    sel.insert(&ev_model);
                break;
            }
            case QGraphicsItem::UserType + 2: // constraint
            {
                const auto& cst_model = static_cast<const ConstraintView*>(item)->presenter().abstractConstraintViewModel().model();
                if(cst_model.parentScenario() == scenario)
                    sel.insert(&cst_model);
                break;
            }
            case QGraphicsItem::UserType + 3: // timenode
            {
                const auto& tn_model = static_cast<const TimeNodeView*>(item)->presenter().model();
                if(tn_model.parentScenario() == scenario)
                    sel.insert(&tn_model);
                break;
            }
            case QGraphicsItem::UserType + 4: // state
            {
                const auto& st_model = static_cast<const StateView*>(item)->presenter().model();
                if(st_model.parentScenario() == scenario)
                {
                    sel.insert(&st_model);
                }
                break;
            }
            default:
                break;
        }
    }

    dispatcher.setAndCommit(filterSelections(sel,
                                             m_parentSM.model().selectedChildren(),
                                             multiSelection()));
}
