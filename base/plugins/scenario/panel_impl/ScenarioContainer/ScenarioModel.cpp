#include "ScenarioModel.hpp"

#include <QDebug>
#include "panel_impl/TimeBox/TimeBoxModel.hpp"
#include "panel_impl/TimeEvent/TimeEventModel.hpp"

ScenarioModel::ScenarioModel (int modelId, QObject* parent) :
	QObject (nullptr),
	m_id {modelId}
{
	setObjectName ("ScenarioModel");
	setParent (parent);

	/*
	QObject* parent_test = this;

	while (parent_test != nullptr)
	{
		qDebug() << parent_test->objectName();
		parent_test = parent_test->parent();
	}
	*/
}

ScenarioModel::~ScenarioModel()
{

}

int ScenarioModel::id() const
{
	return m_id;
}

void ScenarioModel::removeTimeEvent(int modelIndex)
{
	auto model = std::find_if(std::begin(timeEvents()),
							  std::end(timeEvents()),
							  [&modelIndex] (TimeEventModel* elt)
								  {
									  return elt->index() == modelIndex;
								  });
	
	if(model != std::end(timeEvents()))
	{
		emit timeEventRemoved(*model);
		
		delete *model;
		m_timeEvents.erase(model);
	}
}

int ScenarioModel::addTimeEvent(int id, QPointF p)
{
	auto model = new TimeEventModel(id, p.x(), p.y(), "TimeEvent", nullptr);
	emit timeEventAdded(model);
}