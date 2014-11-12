#include "ScenarioModel.hpp"

#include <QDebug>

ScenarioModel::ScenarioModel(int modelId, QObject *parent) :
	QObject(nullptr),
	m_id{modelId}
{
	setObjectName("ScenarioModel");
	setParent(parent);
	
	QObject* parent_test = this;
	
	while(parent_test != nullptr)
	{
		qDebug() << parent_test->objectName();
		parent_test = parent_test->parent();
	}
}

ScenarioModel::~ScenarioModel()
{
	
}

int ScenarioModel::id() const
{
	return m_id;
}

void ScenarioModel::addTimeEvent(QPointF pos)
{
	emit TimeEventAddedInModel(pos);
}

void ScenarioModel::removeTimeEvent(QPointF pos)
{
	emit TimeEventRemovedInModel(pos);
}
