#include "HelloWorldProcessModel.hpp"
#include <QDebug>
using namespace iscore;


HelloWorldProcessModel::HelloWorldProcessModel(unsigned int id, QObject* parent) :
	iscore::ProcessModel{id, parent}
{
	this->setObjectName("HelloWorldProcessModel");
	qDebug("The TestModel begins.");
	qDebug() << m_processText;
}

HelloWorldProcessModel::~HelloWorldProcessModel()
{
	qDebug("The TestModel ends.");
	qDebug() << m_processText;
}
