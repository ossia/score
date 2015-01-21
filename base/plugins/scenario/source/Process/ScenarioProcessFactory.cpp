#include "ScenarioProcessFactory.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessView.hpp"
#include "Process/Temporal/TemporalScenarioProcessPresenter.hpp"

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
		return new TemporalScenarioProcessView{static_cast<QGraphicsObject*>(parent)};

	return nullptr;
}

ProcessPresenterInterface*
ScenarioProcessFactory::makePresenter(ProcessViewModelInterface* pvm,
									  ProcessViewInterface* view,
									  QObject* parent)
{
	return new TemporalScenarioProcessPresenter{pvm, view, parent};
}

ProcessSharedModelInterface* ScenarioProcessFactory::makeModel(id_type<ProcessSharedModelInterface> id, QObject* parent)
{
	return new ScenarioProcessSharedModel{id, parent};
}

#include <interface/serialization/DataStreamVisitor.hpp>
