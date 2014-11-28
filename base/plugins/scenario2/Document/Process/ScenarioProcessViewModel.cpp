#include "ScenarioProcessViewModel.hpp"


ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, int sharedProcessId, QObject* parent):
	iscore::ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, sharedProcessId)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, QObject* parent):
	iscore::ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, -1)
{
	s >> static_cast<iscore::ProcessViewModelInterface&>(*this);

	this->setParent(parent);
}
