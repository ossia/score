#include "ScenarioContainer.hpp"
#include "ScenarioModel.hpp"
#include "ScenarioPresenter.hpp"
#include "ScenarioView.hpp"
#include "panel_impl/TimeEvent/TimeEvent.hpp"
#include "panel_impl/TimeEvent/TimeEventModel.hpp"

#include <QGraphicsScene>
#include <QGraphicsObject>

int ScenarioContainer::m_modelId = 1;

ScenarioContainer::ScenarioContainer (QObject* parent, QGraphicsObject* parentView) :
	QNamedObject {parent, "ScenarioContainer"},
_pModel {new ScenarioModel (m_modelId++, this) },
_pView {new ScenarioView (parentView) },
_pPresenter {new ScenarioPresenter (_pModel, _pView, this) }

{
}

ScenarioContainer::~ScenarioContainer()
{
	
}

