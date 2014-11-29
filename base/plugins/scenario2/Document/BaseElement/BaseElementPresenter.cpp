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
	DocumentDelegatePresenterInterface{parent_presenter, model, view},
	m_baseIntervalPresenter{}
{
	auto cmd = new AddProcessToIntervalCommand(
		{
			"BaseElementModel",
			{
				{"IntervalModel", -1}
			}
		},
		"Scenario");
	cmd->redo();

	auto cmd2 = new CreatEventCommand(
		{
			"BaseElementModel",
			{
				{"IntervalModel", -1},
				{"ScenarioProcessSharedModel", 0}
			}
		},
		150);
	cmd2->redo();

	/*
	QTimer* t = new QTimer;
	connect(t, &QTimer::timeout,
			[cmd] () { cmd->undo(); });
	t->start(5000);
	*/
	m_baseIntervalPresenter = new IntervalPresenter{this->model()->intervalModel(),
													this->view()->intervalView(),
													this};

	this->view()->intervalView()->m_rect.setX(0);
	this->view()->intervalView()->m_rect.setY(0);
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
