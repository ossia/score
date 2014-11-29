#include "ScenarioProcessViewModel.hpp"

#include <Interval/IntervalContent/Storey/StoreyModel.hpp>
#include <Interval/IntervalModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>

ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, int sharedProcessId, QObject* parent):
	iscore::ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, sharedProcessId)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, QObject* parent):
	iscore::ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, -1) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<iscore::ProcessViewModelInterface&>(*this);

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
