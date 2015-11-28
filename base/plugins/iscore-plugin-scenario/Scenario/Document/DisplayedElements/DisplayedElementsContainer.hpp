#pragma once
#include <QPointer>
class ConstraintModel;
class StateModel;
class EventModel;
class TimeNodeModel;
struct DisplayedElementsContainer {
    QPointer<const ConstraintModel> constraint{};
    QPointer<const StateModel> startState{};
    QPointer<const StateModel> endState{};
    QPointer<const EventModel> startEvent{};
    QPointer<const EventModel> endEvent{};
    QPointer<const TimeNodeModel> startNode{};
    QPointer<const TimeNodeModel> endNode{};
};
