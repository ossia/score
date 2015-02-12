#include "InspectorPanel.hpp"
#include "QPushButton"
#include <QLayout>
#include <QDebug>
#include <QScrollArea>
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include "InspectorInterface/InspectorSectionWidget.hpp"

#include <QApplication>

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
	m_itemInspected = InspectorControl::getInspectorWidget(object);

	m_layout->addWidget(m_itemInspected);
	connect(object, &QObject::destroyed,
			this,	&InspectorPanel::on_itemRemoved);
}

void InspectorPanel::on_itemRemoved()
{
	if(m_itemInspected)
	{
		m_layout->removeWidget(m_itemInspected);
		m_itemInspected->deleteLater();
	}

	if(qApp)
	{
		m_itemInspected = new InspectorWidgetBase{};
	}
}
