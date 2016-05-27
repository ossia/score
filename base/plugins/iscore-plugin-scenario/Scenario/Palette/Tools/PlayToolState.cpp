#include "PlayToolState.hpp"

#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Palette/ScenarioPoint.hpp>

namespace Scenario
{
PlayToolState::PlayToolState(const Scenario::ToolPalette &sm):
    m_sm{sm},
    m_exec{m_sm.context().context.app.components.applicationPlugin<ScenarioApplicationPlugin>().execution()}
{

}

void PlayToolState::on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint)
{
    auto item = m_sm.scene().itemAt(scenePoint, QTransform());
    if(!item)
        return;

    switch(item->type())
    {
        case StateView::static_type():
        {
            const auto& state = static_cast<const StateView*>(item)->presenter().model();

            auto id = state.parent() == &this->m_sm.model()
                    ? state.id()
                    : Id<StateModel>{};
            if(id)
                emit m_exec.playState(m_sm.model(), id);
            break;
        }
        default:
            emit m_exec.playAtDate(scenarioPoint.date);
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
