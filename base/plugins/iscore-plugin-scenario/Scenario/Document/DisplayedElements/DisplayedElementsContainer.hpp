#pragma once
#include <QPointer>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
class QGraphicsItem;
namespace Scenario
{
class FullViewConstraintPresenter;
class StatePresenter;
class EventPresenter;
class TimeNodePresenter;
class TimeNodeModel;
struct DisplayedElementsContainer {

        DisplayedElementsContainer() = default;

        DisplayedElementsContainer(
                const ConstraintModel& cst,
                const StateModel& sst,
                const StateModel& est,
                const EventModel& sev,
                const EventModel& eev,
                const TimeNodeModel& stn,
                const TimeNodeModel& etn):
            constraint{&cst},
            startState{&sst},
            endState{&est},
            startEvent{&sev},
            endEvent{&eev},
            startNode{&stn},
            endNode{&etn}
        {

        }

    QPointer<const ConstraintModel> constraint{};
    QPointer<const StateModel> startState{};
    QPointer<const StateModel> endState{};
    QPointer<const EventModel> startEvent{};
    QPointer<const EventModel> endEvent{};
    QPointer<const TimeNodeModel> startNode{};
    QPointer<const TimeNodeModel> endNode{};
};

struct DisplayedElementsPresenterContainer {

        DisplayedElementsPresenterContainer() = default;

        DisplayedElementsPresenterContainer(
                FullViewConstraintPresenter* cp,
                StatePresenter* s1,
                StatePresenter* s2,
                EventPresenter* e1,
                EventPresenter* e2,
                TimeNodePresenter* t1,
                TimeNodePresenter* t2):
            constraint{cp},
            startState{s1},
            endState{s2},
            startEvent{e1},
            endEvent{e2},
            startNode{t1},
            endNode{t2}
        {

        }

    FullViewConstraintPresenter* constraint{};
    StatePresenter* startState{};
    StatePresenter* endState{};
    EventPresenter* startEvent{};
    EventPresenter* endEvent{};
    TimeNodePresenter* startNode{};
    TimeNodePresenter* endNode{};
};
}
