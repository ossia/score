#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ConstraintPresenter.hpp"
#include "Document/Constraint/ConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Commands/Constraint/Process/AddProcessToConstraintCommand.hpp"
#include "Commands/Scenario/CreateEventCommand.hpp"

#include <QTimer>
using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view},
	m_baseConstraintPresenter{}
{
	auto cmd = new AddProcessToConstraintCommand(
		{
			{"BaseConstraintModel", {}}
		},
		"Scenario");
	cmd->redo();

	m_baseConstraintPresenter = new ConstraintPresenter{this->model()->constraintModel(),
													this->view()->constraintView(),
													this};

	connect(m_baseConstraintPresenter,	&ConstraintPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseConstraintPresenter, &ConstraintPresenter::elementSelected,
			this,					 &BaseElementPresenter::elementSelected);
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
