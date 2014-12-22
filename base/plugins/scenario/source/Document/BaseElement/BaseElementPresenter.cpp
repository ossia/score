#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ConstraintPresenter.hpp"
#include "Document/Constraint/ConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/Deck/AddProcessViewToDeck.hpp"

#include "Commands/Scenario/CreateEventCommand.hpp"

#include <QGraphicsScene>
using namespace iscore;
using namespace Scenario;

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
void testInit(ConstraintModel* m)
{
	using namespace Scenario::Command;

	(new AddProcessToConstraintCommand(
		{
			{"BaseConstraintModel", {}}
		},
		"Scenario"))->redo();
	auto scenarioId = m->processes().front()->id();

	(new AddBoxToConstraint(
		ObjectPath{
			{"BaseConstraintModel", {}}
		}))->redo();
	auto box = m->boxes().front();

	(new AddDeckToBox(
		ObjectPath{
			{"BaseConstraintModel", {}},
			{"BoxModel", box->id()}
		}))->redo();
	auto deckId = box->decks().front()->id();

	(new AddProcessViewToDeck(
		{
			{"BaseConstraintModel", {}},
			{"BoxModel", box->id()},
			{"DeckModel", deckId}
		}, scenarioId))->redo();
}

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view},
	m_baseConstraintPresenter{new ConstraintPresenter{
							  this->model()->constraintModel(),
							  this->view()->constraintView(),
							  this}}
{

	testInit(this->model()->constraintModel());
	on_askUpdate();

	connect(m_baseConstraintPresenter,	&ConstraintPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseConstraintPresenter,	&ConstraintPresenter::elementSelected,
			this,						&BaseElementPresenter::elementSelected);

	connect(m_baseConstraintPresenter,	&ConstraintPresenter::askUpdate,
			this,						&BaseElementPresenter::on_askUpdate);
}

void BaseElementPresenter::on_reset()
{
}

void BaseElementPresenter::on_askUpdate()
{
	view()->update();
}

void BaseElementPresenter::selectAll()
{
	for(auto item : view()->scene()->items())
	{
		item->setSelected(true);
	}
}

void BaseElementPresenter::deselectAll()
{
	for(auto item : view()->scene()->items())
	{
		item->setSelected(false);
	}
}

void BaseElementPresenter::deleteSelection()
{
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
