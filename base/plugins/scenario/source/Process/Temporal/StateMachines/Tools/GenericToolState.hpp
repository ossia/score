#pragma once

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

#include <iscore/command/OngoingCommandManager.hpp>
#include <QObject>
#include <QStateMachine>
#include <QGraphicsScene>

namespace iscore
{
    class SerializableCommand;
}

template<typename Element>
bool isUnderMouse(Element ev, const QPointF& scenePos)
{
    return ev->mapRectToScene(ev->boundingRect()).contains(scenePos);
}

template<typename PresenterArray, typename IdToIgnore>
auto getCollidingModels(const PresenterArray& array, const IdToIgnore& id, const QPointF& scenePoint)
{
    using namespace std;
    QList<typename std::add_pointer<decltype(array[0]->model())>::type> colliding;

    for(const auto& elt : array)
    {
        if((!bool(id) || id != elt->id()) && isUnderMouse(elt->view(), scenePoint))
        {
            colliding.push_back(&elt->model());
        }
    }
    // TODO sort the elements according to their Z pos.

    return colliding;
}



class GenericToolState : public QState
{
    protected:
        template<typename EventFun,
                 typename TimeNodeFun,
                 typename ConstraintFun,
                 typename NothingFun>
        void mapTopItem(
                const QGraphicsItem* pressedItem,
                EventFun&& ev_fun,
                TimeNodeFun&& tn_fun,
                ConstraintFun&& cst_fun,
                NothingFun&& nothing_fun) const
        {
            if(!pressedItem)
            {
                nothing_fun();
                return;
            }

            // Each time :
            // Check if it is an event / timenode / constraint
            // Check if it is in our scenario.
            switch(pressedItem->type())
            {
                case QGraphicsItem::UserType + 1:
                    ev_fun(static_cast<const EventView*>(pressedItem)->presenter().model().id());
                    break;

                case QGraphicsItem::UserType + 2:
                    cst_fun(static_cast<const AbstractConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel()->model()->id());
                    break;

                case QGraphicsItem::UserType + 3:
                    tn_fun(static_cast<const TimeNodeView*>(pressedItem)->presenter().model().id());
                    break;

                default:
                    nothing_fun();
                    break;
            }
        }

        auto itemUnderMouse(const QPointF& point) const
        {
            return m_scene.itemAt(point, QTransform());
        }

    public:
        GenericToolState(const ScenarioStateMachine& sm) :
            m_sm{sm},
            m_scene{*m_sm.presenter().view().scene()}
        {
            auto t_click = make_transition<Press_Transition>(this, this);
            connect(t_click, &QAbstractTransition::triggered,
                    this,    &GenericToolState::on_scenarioPressed);

            auto t_move = make_transition<Move_Transition>(this, this);
            connect(t_move, &QAbstractTransition::triggered,
                    this,   &GenericToolState::on_scenarioMoved);

            auto t_rel = make_transition<Release_Transition>(this, this);
            connect(t_rel, &QAbstractTransition::triggered,
                    this,  &GenericToolState::on_scenarioReleased);

            auto t_cancel = make_transition<Cancel_Transition>(this, this);
            connect(t_cancel, &QAbstractTransition::triggered,
                    [&] () { m_localSM.postEvent(new Cancel_Event); });
        }

        void start()
        { m_localSM.start(); }

    protected:
        virtual void on_scenarioPressed() = 0;
        virtual void on_scenarioMoved() = 0;
        virtual void on_scenarioReleased() = 0;

        QStateMachine m_localSM;
        const ScenarioStateMachine& m_sm;
        const QGraphicsScene& m_scene;
};
