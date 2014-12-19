#include "ScenarioProcessFactory.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/ScenarioProcessView.hpp"
#include "Process/ScenarioProcessPresenter.hpp"

QString ScenarioProcessFactory::name() const
{
	return "Scenario";
}

QStringList ScenarioProcessFactory::availableViews()
{
	return {"Temporal"};
}

ProcessViewInterface* ScenarioProcessFactory::makeView(QString view, QObject* parent)
{
	if(view == "Temporal")
		return new ScenarioProcessView{static_cast<QGraphicsObject*>(parent)};

	return nullptr;
}

ProcessPresenterInterface*
ScenarioProcessFactory::makePresenter(ProcessViewModelInterface* pvm,
									  ProcessViewInterface* view,
									  QObject* parent)
{
	return new ScenarioProcessPresenter(pvm, view, parent);
}

ProcessSharedModelInterface* ScenarioProcessFactory::makeModel(int id, QObject* parent)
{
	return new ScenarioProcessSharedModel(id, parent);
}

ProcessSharedModelInterface* ScenarioProcessFactory::makeModel(QDataStream& data, QObject* parent)
{
	return new ScenarioProcessSharedModel(data, parent);
}
