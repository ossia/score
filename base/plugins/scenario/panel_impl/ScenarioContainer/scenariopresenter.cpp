#include "scenariopresenter.hpp"
#include "scenariomodel.hpp"
#include "scenarioview.hpp"

ScenarioPresenter::ScenarioPresenter(ScenarioModel* model, ScenarioView* view, QObject *parent) :
    QObject{parent},
    _pModel{model},
    _pView{view}
{
    setObjectName("SCenarioPresenter");
}

ScenarioPresenter::~ScenarioPresenter()
{
}
