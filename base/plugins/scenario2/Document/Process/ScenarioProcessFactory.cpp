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

iscore::ProcessViewInterface* ScenarioProcessFactory::makeView(QString view, QObject* parent)
{
	if(view == "Temporal")
		return new ScenarioProcessView{static_cast<QGraphicsObject*>(parent)};

	return nullptr;
}

iscore::ProcessPresenterInterface*
ScenarioProcessFactory::makePresenter(iscore::ProcessViewModelInterface* pvm,
									  iscore::ProcessViewInterface* view,
									  QObject* parent)
{
	return new ScenarioProcessPresenter(pvm, view, parent);
}

iscore::ProcessSharedModelInterface*ScenarioProcessFactory::makeModel(int id, QObject* parent)
{
	return new ScenarioProcessSharedModel(id, parent);
}

iscore::ProcessSharedModelInterface*ScenarioProcessFactory::makeModel(QDataStream& data, QObject* parent)
{
	return new ScenarioProcessSharedModel(data, parent);
}