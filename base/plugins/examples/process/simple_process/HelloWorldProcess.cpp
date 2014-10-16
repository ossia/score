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

std::unique_ptr<iscore::ProcessView> HelloWorldProcess::makeView(QString view)
{
	return std::make_unique<iscore::ProcessView>();
}

std::unique_ptr<iscore::ProcessPresenter> HelloWorldProcess::makePresenter()
{
	return std::make_unique<iscore::ProcessPresenter>();
}

std::unique_ptr<iscore::ProcessModel> HelloWorldProcess::makeModel()
{
	return std::make_unique<HelloWorldProcessModel>();
}
