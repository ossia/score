#include "ScenarioContainer.hpp"
#include "ScenarioModel.hpp"
#include "ScenarioPresenter.hpp"
#include "ScenarioView.hpp"
#include "panel_impl/TimeEvent/TimeEvent.hpp"

#include <QGraphicsScene>
#include <QGraphicsObject>
ScenarioContainer::ScenarioContainer (QObject* parent, QGraphicsObject* parentView) :
	QNamedObject {parent, "ScenarioContainer"},
_pModel {new ScenarioModel (m_modelId++, this) },
_pView {new ScenarioView (parentView) },
_pPresenter {new ScenarioPresenter (_pModel, _pView, this) }

{
	connect(_pPresenter, SIGNAL(addTimeEvent(QPointF)),
			this, SLOT(instantiateTimeEvent(QPointF)));
}

ScenarioContainer::~ScenarioContainer()
{
	
}

void ScenarioContainer::instantiateTimeEvent(QPointF pos)
{
	auto evt = new TimeEvent{this, pos};
	//_pModel->addTimeEventModel(evt->model());
	//_pView->addTimeEventView(evt->view());
	//this->addTimeEventPresenter(evt->pr&esenter());
	//view()->parent()
	m_timeEvent.push_back(evt);
}
