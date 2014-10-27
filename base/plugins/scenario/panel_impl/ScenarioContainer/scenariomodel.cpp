#include "scenariomodel.hpp"

#include <QDebug>

ScenarioModel::ScenarioModel(QObject *parent) :
    QObject(parent)
{
    setObjectName("ScenarioModel");
}

ScenarioModel::~ScenarioModel()
{

}

void ScenarioModel::addTimeEvent(QPointF pos)
{
    emit timeEventAddedInModel(pos);
}
