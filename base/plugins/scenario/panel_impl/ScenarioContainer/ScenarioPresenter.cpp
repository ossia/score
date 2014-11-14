#include "ScenarioPresenter.hpp"
#include "ScenarioModel.hpp"
#include "ScenarioView.hpp"

#include <QDebug>
#include <QApplication>

#include "commands/CreateEventCommand.hpp"
#include <core/presenter/Presenter.hpp>
ScenarioPresenter::ScenarioPresenter (ScenarioModel* model, ScenarioView* view, QObject* parent) :
	QObject {nullptr},
        _pModel {model},
_pView {view}
{
	setObjectName ("ScenarioPresenter");
	setParent (parent);
	connect (_pView, &ScenarioView::viewAskForTimeEvent,
	this, &ScenarioPresenter::on_askTimeEvent);
	connect (_pModel, &ScenarioModel::TimeEventAddedInModel,
	this, &ScenarioPresenter::instantiateTimeEvent);
	connect (_pModel, &ScenarioModel::TimeEventRemovedInModel,
	this, &ScenarioPresenter::removeTimeEventInView);
}

ScenarioPresenter::~ScenarioPresenter()
{

}

void ScenarioPresenter::on_askTimeEvent (QPointF pos)
{
	qDebug (Q_FUNC_INFO);
	// On instancie la commande
	auto cmd = new CreatEventCommand (_pModel->id(), pos);

	// Puis on la fait remonter au présenteur
	auto pres = qApp->findChild<iscore::Presenter*> ("Presenter");
	pres->applyCommand (cmd);

	// send request to the model
	//_pModel->addTimeEvent(pos);
}

void ScenarioPresenter::addTimeEventInView (QPointF pos)
{
	emit addTimeEvent (pos);
}

void ScenarioPresenter::removeTimeEventInView (QPointF pos)
{
	emit removeTimeEvent (pos);
}

void ScenarioPresenter::instantiateTimeEvent (QPointF pos)
{
	addTimeEventInView(pos);
}
