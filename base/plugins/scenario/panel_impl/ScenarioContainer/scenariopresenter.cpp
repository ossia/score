#include "scenariopresenter.hpp"
#include "scenariomodel.hpp"
#include "scenarioview.hpp"

#include <QDebug>

ScenarioPresenter::ScenarioPresenter(ScenarioModel* model, ScenarioView* view, QObject *parent) :
    QObject{parent},
    _pModel{model},
    _pView{view}
{
    setObjectName("ScenarioPresenter");
    connect(_pView, &ScenarioView::viewAskForTimeEvent, this, &ScenarioPresenter::addTimeEventInModel);
    connect(_pModel, &ScenarioModel::timeEventAddedInModel, this, &ScenarioPresenter::addTimeEventInView);
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
