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

template<typename PresenterArray, typename IdToIgnore>
auto getCollidingModels(const PresenterArray& array, const IdToIgnore& id)
{
    using namespace std;

    QList<decltype(array[0]->model())> colliding;
    for(const auto& elt : array)
    {
        if((!bool(id) || id != elt->id()) && elt->view()->isUnderMouse())
        {
            colliding.push_back(elt->model());
        }
    }
    // TODO sort the elements according to their Z pos.

    return colliding;
}



class GenericToolState : public QState
{
    protected:
        template<typename View, typename Array>
        auto getPresenterFromView(const View* v, const Array& arr) const
        {
            auto it = std::find_if(std::begin(arr), std::end(arr),
                                   [&] (const typename Array::value_type& val)
            { return val->view() == v; });
            Q_ASSERT(it != std::end(arr));
            return *it;
        }

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
            if(auto ev = dynamic_cast<const EventView*>(pressedItem))
            {
                ev_fun(getPresenterFromView(ev, m_sm.presenter().events())->model()->id());
            }
            else if(auto tn = dynamic_cast<const TimeNodeView*>(pressedItem))
            {
                tn_fun(getPresenterFromView(tn, m_sm.presenter().timeNodes())->model()->id());
            }
            else if(auto cst = dynamic_cast<const AbstractConstraintView*>(pressedItem))
            {
                cst_fun(getPresenterFromView(cst, m_sm.presenter().constraints())
                            ->abstractConstraintViewModel()->model()->id());
            }
            else
            {
                nothing_fun();
            }
        }

        auto itemUnderMouse(const QPointF& point) const
        {
            return m_sm.presenter().view().scene()->itemAt(point, QTransform());
        }

    public:
        GenericToolState(const ScenarioStateMachine& sm) :
            m_sm{sm}
        {
            // Press
            auto t_click = new ScenarioPress_Transition;
            t_click->setTargetState(this); // Check the tool
            connect(t_click, &QAbstractTransition::triggered,
                    [&] () { on_scenarioPressed(); });
            this->addTransition(t_click);

            // Move
            auto t_move = new ScenarioMove_Transition;
            t_move->setTargetState(this); // Check the tool
            connect(t_move, &QAbstractTransition::triggered,
                    [&] () { on_scenarioMoved(); });
            this->addTransition(t_move);

            // Release
            auto t_rel = new ScenarioRelease_Transition;
            t_rel->setTargetState(this); // Check the tool
            connect(t_rel, &QAbstractTransition::triggered,
                    [&] () { on_scenarioReleased(); });
            this->addTransition(t_rel);


            m_waitState = new QState;
            m_localSM.addState(m_waitState);
            m_localSM.setInitialState(m_waitState);
        }

        void start()
        { m_localSM.start(); }

    protected:
        virtual void on_scenarioPressed() = 0;
        virtual void on_scenarioMoved() = 0;
        virtual void on_scenarioReleased() = 0;

        QStateMachine m_localSM;
        QState* m_waitState;
        const ScenarioStateMachine& m_sm;
};
