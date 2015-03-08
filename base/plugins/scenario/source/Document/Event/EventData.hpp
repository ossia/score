#pragma once

#include <QPointF>
#include <public_interface/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class EventModel;
class TimeNodeModel;

struct EventData
{
    id_type<EventModel> eventClickedId {0}; // id of the clicked event

    int x {0}; // use for : position of the mouse realeased point in event coordinates
    TimeValue dDate {std::chrono::seconds{0}}; // date (-> for model)

    int y {0}; // use for : position of the mouse realeased point in event coordinates
    double relativeY {0.0}; // y scaled with current scenario

    QPointF scenePos {QPointF(0, 0) };  // position of mouse in scene Coordinates

    id_type<TimeNodeModel> endTimeNodeId {0};
};
