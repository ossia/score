#include "ScenarioPresenter.hpp"
#include "ScenarioModel.hpp"
#include "ScenarioView.hpp"

#include <QDebug>

ScenarioPresenter::ScenarioPresenter(ScenarioModel* model, ScenarioView* view, QObject *parent) :
	QObject{nullptr},
	_pModel{model},
	_pView{view}
{
	setObjectName("ScenarioPresenter");
	setParent(parent);
	connect(_pView, &ScenarioView::viewAskForTimeEvent, this, &ScenarioPresenter::addTimeEventInModel);
	connect(_pModel, &ScenarioModel::TimeEventAddedInModel, this, &ScenarioPresenter::addTimeEventInView);
}

ScenarioPresenter::~ScenarioPresenter()
{
}

void ScenarioPresenter::addTimeEventInModel(QPointF pos)
{
	// send request to the model
	_pModel->addTimeEvent(pos);
}

void ScenarioPresenter::addTimeEventInView(QPointF pos)
{
	emit addTimeEvent(pos);
}
