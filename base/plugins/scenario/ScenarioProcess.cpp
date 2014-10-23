#include "ScenarioProcess.hpp"
#include "process_impl/ScenarioProcessModel.hpp"
#include <QDebug>


ScenarioProcess::ScenarioProcess():
	iscore::Process()
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

iscore::ProcessView* ScenarioProcess::makeView(QString view)
{
	return new iscore::ProcessView();
}

iscore::ProcessPresenter* ScenarioProcess::makePresenter()
{
	return new iscore::ProcessPresenter();
}

iscore::ProcessModel* ScenarioProcess::makeModel(unsigned int id, QObject* parent)
{
	return new ScenarioProcessModel(id, parent);
}
