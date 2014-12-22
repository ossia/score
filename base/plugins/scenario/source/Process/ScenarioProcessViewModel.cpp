#include "ScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, int sharedProcessId, QObject* parent):
	ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, sharedProcessId)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, QObject* parent):
	ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, -1) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	qDebug() << Q_FUNC_INFO;
	s >> static_cast<ProcessViewModelInterface&>(*this);

	this->setParent(parent);
}

ScenarioProcessSharedModel* ScenarioProcessViewModel::model()
{
	auto parent_constraint = parentConstraint();
	if(parent_constraint)
	{
		return static_cast<ScenarioProcessSharedModel*>(parent_constraint->process(sharedProcessId()));
	}

	return nullptr;
}

// This breaks encapsulation :'(
ConstraintModel* ScenarioProcessViewModel::parentConstraint() const
{
	auto deck = dynamic_cast<DeckModel*>(parent());
	if(deck)
	{
		return deck->parentConstraint();
	}

	return nullptr;
}
