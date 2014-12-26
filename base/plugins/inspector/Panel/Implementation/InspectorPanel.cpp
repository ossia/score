#include "InspectorPanel.hpp"
#include "ui_InspectorPanel.h"
#include "QPushButton"
#include <QLayout>
#include <QDebug>
#include <QScrollArea>
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include "InspectorInterface/InspectorSectionWidget.hpp"

#include <core/plugin/PluginManager.hpp>
#include "InspectorControl.hpp"
InspectorPanel::InspectorPanel (QWidget* parent) :
	QWidget {parent},
	m_layout{new QVBoxLayout{this}}
{
	m_layout->setMargin (8);

	setMinimumWidth (300);
	setMaximumHeight (800);
}

InspectorPanel::~InspectorPanel()
{
}

void InspectorPanel::newItemInspected (QObject* object)
{
	delete m_itemInspected;
	auto pmgr = qApp->findChild<InspectorControl*>("InspectorControl");

	// TODO do like Scenario.

	auto factories = pmgr->factories();

	for(auto factory : factories)
	{
		if(factory->correspondingObjectName() == object->objectName())
		{
			m_itemInspected = factory->makeWidget(object); // TODO multiple items.

			// Note : private method QLayout::addItem takes ownership
			m_layout->addWidget(m_itemInspected);

			connect(object, &QObject::destroyed,
					this,	&InspectorPanel::on_itemRemoved);

			return;
		}
	}

	// When no factory is found.
	m_itemInspected = new InspectorWidgetBase(object);
}

void InspectorPanel::on_itemRemoved()
{
	delete m_itemInspected;
	if(qApp)
		m_itemInspected = new InspectorWidgetBase{};
}
