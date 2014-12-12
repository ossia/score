#include "ScenarioProcessViewModel.hpp"

#include "Document/Interval/IntervalContent/Storey/StoreyModel.hpp"
#include "Document/Interval/IntervalModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, int sharedProcessId, QObject* parent):
	ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, sharedProcessId)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, QObject* parent):
	ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, -1) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<ProcessViewModelInterface&>(*this);

	this->setParent(parent);

}

ScenarioProcessSharedModel* ScenarioProcessViewModel::model()
{
	auto parent_interval = parentInterval();
	if(parent_interval)
	{
		return static_cast<ScenarioProcessSharedModel*>(parent_interval->process(sharedProcessId()));
	}

	return nullptr;
}

IntervalModel* ScenarioProcessViewModel::parentInterval() const
{
	auto storey = dynamic_cast<StoreyModel*>(parent());
	if(storey)
	{
		return storey->parentInterval();
	}

	return nullptr;
}
