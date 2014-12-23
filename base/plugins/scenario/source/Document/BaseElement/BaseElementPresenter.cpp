#include "BaseElementPresenter.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/Deck/AddProcessViewToDeck.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"

#include "Commands/Scenario/CreateEventCommand.hpp"

#include <QGraphicsScene>
using namespace iscore;
using namespace Scenario;

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
void testInit(TemporalConstraintViewModel* viewmodel)
{
	using namespace Scenario::Command;
	auto constraint_model = viewmodel->model();

	(new AddProcessToConstraintCommand(
		{
			{"BaseConstraintModel", {}}
		},
		"Scenario"))->redo();
	auto scenarioId = constraint_model->processes().front()->id();

	(new AddBoxToConstraint(
		ObjectPath{
			{"BaseConstraintModel", {}}
		}))->redo();
	auto box = constraint_model->boxes().front();

	(new ShowBoxInViewModel(viewmodel, box->id()))->redo();

	(new AddDeckToBox(
		ObjectPath{
			{"BaseConstraintModel", {}},
			{"BoxModel", box->id()}
		}))->redo();
	auto deckId = box->decks().front()->id();

	(new AddProcessViewModelToDeck(
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
	m_baseConstraintPresenter{new TemporalConstraintPresenter{
							  this->model()->constraintViewModel(),
							  this->view()->constraintView(),
							  this}}
{

	testInit(this->model()->constraintViewModel());
	on_askUpdate();

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::elementSelected,
			this,						&BaseElementPresenter::elementSelected);

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::askUpdate,
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
