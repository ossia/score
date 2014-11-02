#include "scenariocontainer.hpp"
#include "scenariomodel.hpp"
#include "scenariopresenter.hpp"
#include "scenarioview.hpp"

ScenarioContainer::ScenarioContainer(QObject *parent , QGraphicsObject *parentView) :
	QObject{parent},
	_pModel{new ScenarioModel(this)},
	_pView {new ScenarioView(parentView)},
	_pPresenter {new ScenarioPresenter(_pModel, _pView, this)}

{
	setObjectName("ScenarioContainer");
	setParent(parent);
}

ScenarioContainer::~ScenarioContainer()
{

}
