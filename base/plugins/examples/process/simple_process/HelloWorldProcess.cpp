#include "HelloWorldProcess.hpp"
#include "process_impl/HelloWorldProcessModel.hpp"
#include <QDebug>


HelloWorldProcess::HelloWorldProcess():
	iscore::Process()
{
	qDebug("Successfully instantiated HelloWorldProcess");
}

QString HelloWorldProcess::name() const
{
	return "HelloWorld Example Process";
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

iscore::ProcessModel* HelloWorldProcess::makeModel(unsigned int id, QObject* parent)
{
	return new HelloWorldProcessModel(id, parent);
}
