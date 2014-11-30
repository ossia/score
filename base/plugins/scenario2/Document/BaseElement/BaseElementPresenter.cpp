#include "BaseElementPresenter.hpp"
#include <Document/Interval/IntervalPresenter.hpp>
#include <Document/Interval/IntervalView.hpp>
#include <Document/BaseElement/BaseElementModel.hpp>
#include <Document/BaseElement/BaseElementView.hpp>
#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>
#include <Commands/Scenario/CreateEventCommand.hpp>
#include <Commands/Scenario/CreateIntervalCommand.hpp>

#include <QTimer>
using namespace iscore;

BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view},
	m_baseIntervalPresenter{}
{
	this->setObjectName("BaseElementPresenter");
	this->setParent(parent_presenter);
	auto cmd = new AddProcessToIntervalCommand(
		{
			"BaseElementModel",
			{
				{"BaseIntervalModel", -1}
			}
		},
		"Scenario");
	cmd->redo();
/*
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

	auto cmd3 = new CreateEventAfterEventCommand(
	{
		"BaseElementModel",
		{
			{"IntervalModel", -1},
			{"ScenarioProcessSharedModel", 0}
		}
	},
	1,
	400);
	//cmd3->redo();

	QTimer* t = new QTimer; t->setSingleShot(true);

	QTimer* t2 = new QTimer; t2->setSingleShot(true);
	connect(t, &QTimer::timeout, [=] () { cmd3->redo(); this->view()->intervalView()->update(); t2->start(300);});
	connect(t2, &QTimer::timeout, [=] () { cmd3->undo(); this->view()->intervalView()->update(); t->start(300); });

	t->start(300);
	*/


	m_baseIntervalPresenter = new IntervalPresenter{this->model()->intervalModel(),
													this->view()->intervalView(),
													this};


	connect(this, &BaseElementPresenter::submitCommand,
			[ ](iscore::SerializableCommand*) { qDebug(Q_FUNC_INFO); });
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
