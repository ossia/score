#pragma once

#include <QWidget>

class QPushButton;
namespace Scenario
{
class EventModel;
class EventSummaryWidget final : public QWidget
{
    public:
	EventSummaryWidget( const EventModel& object, QWidget* parent = 0);

};
}
