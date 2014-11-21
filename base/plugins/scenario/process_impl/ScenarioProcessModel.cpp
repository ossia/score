#include "ScenarioProcessModel.hpp"
#include <QDebug>
using namespace iscore;


ScenarioProcessModel::ScenarioProcessModel(unsigned int id, QObject* parent) :
	iscore::ProcessSharedModelInterface{id, parent}
{
	this->setObjectName("ScenarioProcessModel");
	qDebug("The TestModel begins.");
	qDebug() << m_processText;
}

ScenarioProcessModel::~ScenarioProcessModel()
{
	qDebug("The TestModel ends.");
	qDebug() << m_processText;
}


ProcessViewModelInterface*ScenarioProcessModel::makeViewModel(int id, QObject* parent)
{
}