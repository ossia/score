#include "BaseElementModel.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
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
void testInit(FullViewConstraintViewModel* viewmodel)
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

	ShowBoxInViewModel cmd3{viewmodel, box->id()};
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
		}, scenarioId};
	cmd5.redo();
}

BaseElementModel::BaseElementModel(QByteArray data, QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent},
	m_baseConstraint{new ConstraintModel{Deserializer<DataStream>{&data}, this}}
{
	m_baseConstraint->setObjectName("BaseConstraintModel");

	setDisplayedConstraint(m_baseConstraint);
}

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent},
	m_baseConstraint{new ConstraintModel{id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, 0, this}}
{
	m_baseConstraint->setDefaultDuration(1000);
	m_baseConstraint->setObjectName("BaseConstraintModel");
	testInit(m_baseConstraint->fullView());

	setDisplayedConstraint(m_baseConstraint);
}

FullViewConstraintViewModel *BaseElementModel::constraintViewModel() const
{
	return m_baseConstraint->fullView();
}

QByteArray BaseElementModel::save()
{
	QByteArray arr;
	Serializer<DataStream> s{&arr};
	s.readFrom(*constraintModel());

	return arr;
}

QJsonObject BaseElementModel::toJson()
{
	Serializer<JSON> s;
	s.readFrom(*constraintModel());

	return s.m_obj;
}

void BaseElementModel::setDisplayedConstraint(ConstraintModel* c)
{
	if(c && c != m_displayedConstraint)
	{
		m_displayedConstraint = c;
		emit displayedConstraintChanged();
	}
}

void BaseElementModel::setDisplayedObject(ObjectPath path)
{
	if(path.vec().last().objectName() == "ConstraintModel" || path.vec().last().objectName() == "BaseConstraintModel")
	{
		setDisplayedConstraint(path.find<ConstraintModel>());
	}
}
