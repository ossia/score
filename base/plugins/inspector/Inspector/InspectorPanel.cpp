#include "InspectorPanel.hpp"
#include "ui_InspectorPanel.h"
#include "QPushButton"
#include <QLayout>
#include <QDebug>
#include <QScrollArea>

#include "../Interval/IntervalInspectorview.hpp"
#include "InspectorSectionWidget.hpp"
#include <Interval/InspectorWidget.hpp>

InspectorPanel::InspectorPanel (QWidget* parent) :
	QWidget (parent)
{

	_layout = new QVBoxLayout (this);
	_layout->setMargin (8);

	setMinimumWidth (300);
	setMaximumHeight (800);
}

InspectorPanel::~InspectorPanel()
{
}

void InspectorPanel::newItemInspected (QObject* object)
{
	if (itemInspected != nullptr)
	{
		delete itemInspected;
	}

	// todo : switch on cast result

	// Demo
	InspectorWidget* factory = new InspectorWidget;
	InspectorWidgetInterface* item = factory->makeWidget (object);

	_layout->addWidget (item);
}
