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

iscore::ProcessViewInterface* ScenarioProcess::makeView(QString view, QObject* parent)
{
	return nullptr;
}

iscore::ProcessPresenterInterface* ScenarioProcess::makePresenter(iscore::ProcessViewModelInterface* pvm,
																  iscore::ProcessViewInterface* view,
																  QObject* parent)
{
	return nullptr; //new iscore::ProcessPresenterInterface(nullptr, "");
}

iscore::ProcessSharedModelInterface* ScenarioProcess::makeModel(unsigned int id, QObject* parent)
{
	return nullptr;//new ScenarioProcessModel(id, parent);
}

iscore::ProcessSharedModelInterface* ScenarioProcess::makeModel(QDataStream& ar, QObject* parent)
{
	return nullptr; //new ScenarioProcessModel(id, parent);
}
