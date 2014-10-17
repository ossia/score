#include "HelloWorldProcess.hpp"
#include "process_impl/HelloWorldProcessModel.hpp"
#include <QDebug>


HelloWorldProcess::HelloWorldProcess():
	iscore::Process()
{
	qDebug("Successfully instantiated HelloWorldProcess");
}

QStringList HelloWorldProcess::availableViews()
{
	return {};
}

iscore::ProcessView* HelloWorldProcess::makeView(QString view)
{
	return new iscore::ProcessView();
}

iscore::ProcessPresenter* HelloWorldProcess::makePresenter()
{
	return new iscore::ProcessPresenter();
}

iscore::ProcessModel* HelloWorldProcess::makeModel()
{
	return new HelloWorldProcessModel();
}
