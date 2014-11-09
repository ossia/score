#include "ScenarioProcess.hpp"
#include "process_impl/ScenarioProcessModel.hpp"
#include <interface/process/ProcessPresenterInterface.hpp>
#include <interface/process/ProcessViewInterface.hpp>
#include <QDebug>


ScenarioProcess::ScenarioProcess():
	iscore::ProcessFactoryInterface()
{
	qDebug("Successfully instantiated ScenarioProcess");
}

QString ScenarioProcess::name() const
{
	return "Scenario Example Process";
}

QStringList ScenarioProcess::availableViews()
{
	return {};
}

iscore::ProcessViewInterface* ScenarioProcess::makeView(QString view)
{
	return new iscore::ProcessViewInterface();
}

iscore::ProcessPresenterInterface* ScenarioProcess::makePresenter()
{
	return new iscore::ProcessPresenterInterface();
}

iscore::ProcessModelInterface* ScenarioProcess::makeModel(unsigned int id, QObject* parent)
{
	return new ScenarioProcessModel(id, parent);
}
