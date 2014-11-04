#include "ScenarioModel.hpp"

#include <QDebug>

ScenarioModel::ScenarioModel(QObject *parent) :
	QObject(nullptr)
{
	setObjectName("ScenarioModel");
	setParent(parent);
}

ScenarioModel::~ScenarioModel()
{

}

void ScenarioModel::addTimeEvent(QPointF pos)
{
	emit TimeEventAddedInModel(pos);
}
