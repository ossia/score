#pragma once
#include <QObject>
#include <iscore/command/OngoingCommandManager.hpp>
#include <Document/Event/EventData.hpp>
#include <QStateMachine>
class TemporalScenarioPresenter;

namespace iscore
{
    class SerializableCommand;
}

struct ConstraintData;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class QPointF;

#include "StateMachines/CreateEventStateMachine.hpp"
class ScenarioCommandManager : public QObject
{
    public:
        ScenarioCommandManager(TemporalScenarioPresenter& presenter);

        void createConstraint(EventData);
        void on_scenarioPressed(const QPointF& point);
        void on_scenarioMoved(const QPointF& point);
        void on_scenarioReleased(const QPointF& point);

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

        EventData m_lastData {};
        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;
        LockingOngoingCommandDispatcher<MergeStrategy::Simple>* m_creationCommandDispatcher{};
        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo>* m_moveCommandDispatcher{};
        CommandDispatcher<SendStrategy::Simple>* m_instantCommandDispatcher{};


        CreateEventState* m_createEvent;

        QStateMachine m_sm;
};
