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
	return new TemporalScenarioProcessPresenter(pvm, view, parent);
}

ProcessSharedModelInterface* ScenarioProcessFactory::makeModel(int id, QObject* parent)
{
	return new ScenarioProcessSharedModel(id, parent);
}

#include <interface/serialization/DataStreamVisitor.hpp>

ProcessSharedModelInterface*ScenarioProcessFactory::makeModel(SerializationIdentifier identifier,
															  void* data,
															  QObject* parent)
{
	if(identifier == DataStream::type())
	{
		return new ScenarioProcessSharedModel{*static_cast<Deserializer<DataStream>*>(data), parent};
	}

	throw std::runtime_error("ScenarioSharedProcessModel only supports DataStream serialization");
}
