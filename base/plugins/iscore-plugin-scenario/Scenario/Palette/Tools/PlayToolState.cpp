#include "PlayToolState.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Palette/ScenarioPoint.hpp>
#include <QKeyEvent>
#include <QApplication>
namespace Scenario
{
PlayToolState::PlayToolState(const Scenario::ToolPalette& sm)
    : m_sm{sm}
    , m_exec{m_sm.context()
                 .context.app
                 .applicationPlugin<ScenarioApplicationPlugin>()
                 .execution()}
{
}

void PlayToolState::on_pressed(
    QPointF scenePoint, Scenario::Point scenarioPoint)
{
  auto item = qobject_cast<GraphicsItem*>(m_sm.scene().childAt(scenePoint.x(), scenePoint.y()));
  if (!item)
    return;

  switch (item->type())
  {
    case StateView::static_type():
    {
      const auto& state
          = safe_cast<const StateView*>(item)->presenter().model();

      auto id = state.parent() == &this->m_sm.model()
                    ? state.id()
                    : OptionalId<StateModel>{};
      if (id)
        emit m_exec.playState(m_sm.model(), *id);
      break;
    }
    case ConstraintView::static_type():
    {
      const auto& cst
          = safe_cast<const ConstraintView*>(item)->presenter().model();

      auto id = cst.parent() == &this->m_sm.model()
                    ? cst.id()
                    : OptionalId<ConstraintModel>{};
      if (id)
      {
        if(QApplication::keyboardModifiers() & Qt::AltModifier)
        {
          emit m_exec.playConstraint(m_sm.model(), *id);
        }
        else
        {
          emit m_exec.playFromConstraintAtDate(
                m_sm.model(),
                *id,
                scenarioPoint.date);
        }
      }
      break;
    }
      // TODO Play constraint ? the code is already here.
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
