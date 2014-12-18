#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ConstraintPresenter.hpp"
#include "Document/Constraint/ConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Commands/Constraint/Process/AddProcessToConstraintCommand.hpp"
#include "Commands/Scenario/CreateEventCommand.hpp"

#include <QGraphicsScene>
using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view},
	m_baseConstraintPresenter{new ConstraintPresenter{
							  this->model()->constraintModel(),
							  this->view()->constraintView(),
							  this}}
{
	auto cmd = new AddProcessToConstraintCommand(
		{
			{"BaseConstraintModel", {}}
		},
		"Scenario");
	cmd->redo();

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
	// 1. List all the things to be deleted.

	// 2. Remove the children of things contained in other things from the list

	// 3. Create a Delete command for each.

	// 4. Make a meta-command that binds them all and calls undo & redo on the queue.
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
