#include "BaseElementModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include <QJsonDocument>
#include <interface/serialization/JSONVisitor.hpp>

#include <iostream>

#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"
#include "Commands/Scenario/CreateEvent.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace Scenario;
void testInit(TemporalConstraintViewModel* viewmodel)
{
	using namespace Scenario::Command;
	auto constraint_model = viewmodel->model();

	AddProcessToConstraint cmd1{
		{
			{"BaseConstraintModel", {}}
		},
		"Scenario"};
	cmd1.redo();
	auto scenarioId = constraint_model->processes().front()->id();

	AddBoxToConstraint cmd2{
		ObjectPath{
			{"BaseConstraintModel", {}}
		}};
	cmd2.redo();
	auto box = constraint_model->boxes().front();

	ShowBoxInViewModel cmd3{viewmodel,
							(SettableIdentifier::identifier_type)box->id()};
	cmd3.redo();

	AddDeckToBox cmd4{
		ObjectPath{
			{"BaseConstraintModel", {}},
			{"BoxModel", box->id()}
		}};
	cmd4.redo();
	auto deckId = box->decks().front()->id();

	AddProcessViewModelToDeck cmd5{
		{
			{"BaseConstraintModel", {}},
			{"BoxModel", box->id()},
			{"DeckModel", deckId}
		}, (SettableIdentifier::identifier_type)scenarioId};
	cmd5.redo();
}

BaseElementModel::BaseElementModel(QByteArray data, QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent}
{
	Deserializer<DataStream> deserializer{&data};
	m_baseConstraint = new ConstraintModel{deserializer, this};
	m_baseConstraint->setObjectName("BaseConstraintModel");
	m_viewModel = m_baseConstraint->makeConstraintViewModel<TemporalConstraintViewModel>(0, m_baseConstraint);


	(new Command::ShowBoxInViewModel(m_viewModel,
									 (SettableIdentifier::identifier_type)m_baseConstraint->boxes().front()->id()))->redo();
}

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent},
	m_baseConstraint{new ConstraintModel{0, this}},
	m_viewModel{m_baseConstraint->makeConstraintViewModel<TemporalConstraintViewModel>(0, m_baseConstraint)}
{
    m_baseConstraint->setDefaultDuration(1000);
    m_baseConstraint->setObjectName("BaseConstraintModel");
	testInit(m_viewModel);
}

QByteArray BaseElementModel::save()
{
	QByteArray arr;
	Serializer<DataStream> s{&arr};
	s.readFrom(*constraintModel());

	return arr;
	//QJsonDocument doc;
	//doc.setObject(s.m_obj);
	//return doc.toJson();
}
