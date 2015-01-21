#pragma once

#include <QPointF>
#include <tools/SettableIdentifierAlternative.hpp>

class EventModel;

struct EventData
{
	id_type<EventModel> eventClickedId{0};  // id of the clicked event

	int x{0};  // use for : position of the mouse realeased point in event coordinates
	int dDate{0}; // date (-> for model)

	int y{0};  // idem
	double relativeY{0.0}; // y scaled with current scenario

	QPointF scenePos{QPointF(0,0)}; // position of mouse in scene Coordinates
};
