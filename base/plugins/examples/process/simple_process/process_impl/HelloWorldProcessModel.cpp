#include "HelloWorldProcessModel.hpp"
#include <QDebug>
using namespace iscore;


HelloWorldProcessModel::HelloWorldProcessModel(unsigned int id, QObject* parent) :
	iscore::ProcessSharedModelInterface{parent, "HelloWorldProcessModel", id}
{
	this->setObjectName("HelloWorldProcessModel");
	qDebug("The TestModel begins.");
	qDebug() << m_processText;
}

HelloWorldProcessModel::HelloWorldProcessModel(unsigned int id, QByteArray arr, QObject* parent) :
	iscore::ProcessSharedModelInterface{parent, "HelloWorldProcessModel", id}
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


ProcessViewModelInterface*HelloWorldProcessModel::makeViewModel(int id, QObject* parent)
{
}