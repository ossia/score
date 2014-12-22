#include "ScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, model)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, model) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<ProcessViewModelInterface&>(*this);
	this->setParent(parent);
}
