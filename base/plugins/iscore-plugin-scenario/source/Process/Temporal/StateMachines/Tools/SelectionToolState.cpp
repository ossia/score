#include "SelectionToolState.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include <core/document/Document.hpp>

#include <QGraphicsScene>

#include <iscore/statemachine/CommonSelectionState.hpp>

// TODO put in its own file
class ScenarioSelectionState : public CommonSelectionState
{
    private:
        QPointF m_initialPoint;
        QPointF m_movePoint;
        const ScenarioStateMachine& m_parentSM;
        TemporalScenarioView& m_scenarioView;

    public:
        ScenarioSelectionState(
                iscore::SelectionStack& stack,
                const ScenarioStateMachine& parentSM,
                TemporalScenarioView& scenarioview,
                QState* parent):
            CommonSelectionState{stack, &scenarioview, parent},
            m_parentSM{parentSM},
            m_scenarioView{scenarioview}
        {

        }

        const QPointF& initialPoint() const
        { return m_initialPoint; }
        const QPointF& movePoint() const
        { return m_movePoint; }

        void on_pressAreaSelection() override
        {
            m_initialPoint = m_parentSM.scenePoint;
        }

        void on_moveAreaSelection() override
        {
            m_movePoint = m_parentSM.scenePoint;
            m_scenarioView.setSelectionArea(
                        QRectF{m_scenarioView.mapFromScene(m_initialPoint),
                               m_scenarioView.mapFromScene(m_movePoint)}.normalized());
            setSelectionArea(QRectF{m_initialPoint, m_movePoint}.normalized());
        }

        void on_releaseAreaSelection() override
        {
            m_scenarioView.setSelectionArea(QRectF{});
        }

        void on_deselect() override
        {
            dispatcher.setAndCommit(Selection{});
            m_scenarioView.setSelectionArea(QRectF{});
        }

        void on_delete() override
        {
            ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
            mgr.removeSelection(m_parentSM.model());
        }

        void on_deleteContent() override
        {
            ScenarioGlobalCommandManager mgr{m_parentSM.commandStack()};
            mgr.clearContentFromSelection(m_parentSM.model());
        }

        void setSelectionArea(const QRectF& area)
        {
            using namespace std;
            QPainterPath path;
            path.addRect(area);
            Selection sel;

            auto scenario = &m_parentSM.presenter().viewModel().sharedProcessModel();
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
                        const auto& cst_model = static_cast<const AbstractConstraintView*>(item)->presenter().abstractConstraintViewModel().model();
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
                }
            }

            dispatcher.setAndCommit(filterSelections(sel,
                                                       m_parentSM.model().selectedChildren(),
                                                       multiSelection()));
        }
};

SelectionTool::SelectionTool(ScenarioStateMachine& sm):
    ScenarioTool{sm, &sm}
{
    m_state = new ScenarioSelectionState{
            iscore::IDocument::documentFromObject(m_parentSM.model())->selectionStack(),
            m_parentSM,
            m_parentSM.presenter().view(),
            &localSM()};

    localSM().setInitialState(m_state);
}




void SelectionTool::on_pressed()
{
    using namespace std;
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id) // Event
    {
        const auto& elt = m_parentSM.presenter().events().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {
        const auto& elt = m_parentSM.presenter().timeNodes().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {
        const auto& elt = m_parentSM.presenter().constraints().at(id);

        m_state->dispatcher.setAndCommit(filterSelections(&elt->model(),
                                                   m_parentSM.model().selectedChildren(),
                                                   m_state->multiSelection()));
    },
    [&] () { localSM().postEvent(new Press_Event); });
}

void SelectionTool::on_moved()
{
    localSM().postEvent(new Move_Event);
    /*
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>&) {  },
    [&] (const id_type<TimeNodeModel>&) { localSM().postEvent(new Move_Event); },
    [&] (const id_type<ConstraintModel>&) { localSM().postEvent(new Move_Event); },
    [&] () { localSM().postEvent(new Move_Event); });
    */
}

void SelectionTool::on_released()
{
    localSM().postEvent(new Release_Event);
    /*
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>&) {  },
    [&] (const id_type<TimeNodeModel>&) { localSM().postEvent(new Release_Event); },
    [&] (const id_type<ConstraintModel>&) { localSM().postEvent(new Release_Event); },
    [&] () { localSM().postEvent(new Release_Event); });
    */
}


