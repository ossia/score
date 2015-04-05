#pragma once
#include <QObject>
#include <iscore/command/OngoingCommandManager.hpp>
#include "CreateEventStateMachine.hpp"
#include <Document/Event/EventData.hpp>
#include <QStateMachine>
class ScenarioStateMachine;

namespace iscore
{
    class SerializableCommand;
}

struct ConstraintData;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class QPointF;

class CreationToolState : public QState
{
    public:
        CreationToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed();
        void on_scenarioMoved();
        void on_scenarioReleased();

    private:
        template<typename View, typename Array>
        auto getPresenterFromView(const View* v, const Array& arr)
        {
            // TODO : filter the elements currently being created ?
            auto it = std::find_if(begin(arr), end(arr),
                                   [&] (const typename Array::value_type& val)
            { return val->view() == v; });
            Q_ASSERT(it != end(arr));
            return *it;
        }

        template<typename EventFun,
                 typename TimeNodeFun,
                 typename ConstraintFun,
                 typename NothingFun>
        void mapItemToAction(QGraphicsItem* pressedItem,
                             EventFun&& ev_fun,
                             TimeNodeFun&& tn_fun,
                             ConstraintFun&& cst_fun,
                             NothingFun&& nothing_fun);

        auto itemUnderMouse(const QPointF& point);


        QStateMachine ct_sm;
        CreateEventState* m_createEvent;
        ScenarioStateMachine& m_sm;
};
