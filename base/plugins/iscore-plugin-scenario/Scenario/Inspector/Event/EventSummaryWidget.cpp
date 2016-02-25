#include "EventSummaryWidget.hpp"

#include <QToolButton>
#include <QLabel>
#include <QGridLayout>

#include <iscore/widgets/MarginLess.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <State/Expression.hpp>

namespace Scenario
{
EventSummaryWidget::EventSummaryWidget(const EventModel& object, QWidget* parent):
    QWidget{parent}
{
    auto mainLay = new iscore::MarginLess<QGridLayout>{this};
    // browser button
    auto eventBtn = new QToolButton{this};
    eventBtn->setText("+");

    mainLay->addWidget(new QLabel{object.metadata.name()}, 0, 0, 1, 2);
    mainLay->addWidget( new QLabel{object.date().toString()}, 0, 2, 1, 1);
    mainLay->addWidget(eventBtn,0, 3, 1, 1);

    if(!object.condition().toString().isEmpty())
    {
//	mainLay->addWidget(new QLabel{tr("Condition")}, 1, 0, 1, 1);
	mainLay->addWidget(new QLabel{object.condition().toString()}, 1, 1, 1, 3);
    }
}
}
