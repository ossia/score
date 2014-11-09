#include "HelloWorldProcess.hpp"
#include "process_impl/HelloWorldProcessModel.hpp"
#include <interface/process/ProcessPresenterInterface.hpp>
#include <interface/process/ProcessViewInterface.hpp>
#include <QDebug>


HelloWorldProcess::HelloWorldProcess():
	iscore::ProcessFactoryInterface()
{
	//qDebug("Successfully instantiated HelloWorldProcess");
}

QString HelloWorldProcess::name() const
{
	return "HelloWorld Example Process";
}

QStringList HelloWorldProcess::availableViews()
{
	return {};
}

iscore::ProcessViewInterface* HelloWorldProcess::makeView(QString view)
{
	return new iscore::ProcessViewInterface();
}

iscore::ProcessPresenterInterface* HelloWorldProcess::makePresenter()
{
	return new iscore::ProcessPresenterInterface();
}

iscore::ProcessModelInterface* HelloWorldProcess::makeModel(unsigned int id, QObject* parent)
{
	return new HelloWorldProcessModel(id, parent);
}
