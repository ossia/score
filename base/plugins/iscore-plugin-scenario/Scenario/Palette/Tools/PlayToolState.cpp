#include "PlayToolState.hpp"

#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Palette/ScenarioPoint.hpp>

namespace Scenario
{
PlayToolState::PlayToolState(const Scenario::ToolPalette &sm):
    m_sm{sm}
{

}

void PlayToolState::on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint)
{
    auto item = m_sm.scene().itemAt(scenePoint, QTransform());
    if(!item)
	return;

    auto& app =  m_sm.context().app.components.applicationPlugin<ScenarioApplicationPlugin>();

    switch(item->type())
    {
	case StateView::static_type():
	    {
		const auto& state = static_cast<const StateView*>(item)->presenter().model();

		auto id = state.parent() == &this->m_sm.model()
			? state.id()
			: Id<StateModel>{};
		if(id)
		    emit app.playState(id);
		break;
	    }
	default:
	    emit app.playAtDate(scenarioPoint.date);
	    break;
    }
}

void PlayToolState::on_moved()
{

}

void PlayToolState::on_released()
{

}

}
