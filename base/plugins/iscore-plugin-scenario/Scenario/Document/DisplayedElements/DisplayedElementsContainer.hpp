#pragma once
#include <QPointer>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

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
