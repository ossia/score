#include "BaseElementPresenter.hpp"
#include <Document/Interval/IntervalPresenter.hpp>
#include <Document/Interval/IntervalView.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>
#include <Document/BaseElement/BaseElementView.hpp>
#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>
#include <Commands/Scenario/CreateEventCommand.hpp>

#include <QTimer>
using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view},
	m_baseIntervalPresenter{}
{
	auto cmd = new AddProcessToIntervalCommand(
		{
			{"BaseIntervalModel", {}}
		},
		"Scenario");
	cmd->redo();

	m_baseIntervalPresenter = new IntervalPresenter{this->model()->intervalModel(),
													this->view()->intervalView(),
													this};

	connect(m_baseIntervalPresenter,	&IntervalPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseIntervalPresenter, &IntervalPresenter::elementSelected,
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
