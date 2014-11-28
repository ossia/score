#include "ScenarioProcessFactory.hpp"
#include "ScenarioProcessSharedModel.hpp"
#include "ScenarioProcessView.hpp"
#include "ScenarioProcessPresenter.hpp"

QString ScenarioProcessFactory::name() const
{
	return "Scenario";
}

QStringList ScenarioProcessFactory::availableViews()
{
	return {"Temporal"};
}

iscore::ProcessViewInterface*ScenarioProcessFactory::makeView(QString view)
{
	return new ScenarioProcessView;
}

iscore::ProcessPresenterInterface*ScenarioProcessFactory::makePresenter()
{
	return new ScenarioProcessPresenter;
}

iscore::ProcessSharedModelInterface*ScenarioProcessFactory::makeModel(unsigned int id, QObject* parent)
{
	return new ScenarioProcessSharedModel(id, parent);
}

iscore::ProcessSharedModelInterface*ScenarioProcessFactory::makeModel(QDataStream& data, QObject* parent)
{
	return new ScenarioProcessSharedModel(data, parent);
}