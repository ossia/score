// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayToolState.hpp"

#include <QApplication>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
namespace Scenario
{
PlayToolState::PlayToolState(const Scenario::ToolPalette& sm)
    : m_sm{sm}
    , m_exec{m_sm.context()
                 .context.app.guiApplicationPlugin<ScenarioApplicationPlugin>()
                 .execution()}
{
}

void PlayToolState::on_pressed(
    QPointF scenePoint,
    Scenario::Point scenarioPoint)
{
  auto item = m_sm.scene().itemAt(scenePoint, QTransform());
  if (!item)
    return;

  switch (item->type())
  {
    case StateView::Type:
    {
      const auto& state
          = safe_cast<const StateView*>(item)->presenter().model();

      auto id = state.parent() == &this->m_sm.model()
                    ? state.id()
                    : OptionalId<StateModel>{};
      if (id)
        m_exec.playState(&m_sm.model(), *id);
      break;
    }
    case IntervalHeader::Type:
      item = safe_cast<const IntervalHeader*>(item)->intervalView();
      [[fallthrough]];
    case IntervalView::Type:
    {
      const auto& cst
          = safe_cast<const IntervalView*>(item)->presenter().model();

      if (cst.parent() == &this->m_sm.model())
      {
        if (QApplication::keyboardModifiers() & Qt::AltModifier)
        {
          m_exec.playInterval(const_cast<IntervalModel*>(&cst));
        }
        else
        {
          m_exec.playFromIntervalAtDate(
              &m_sm.model(), cst.id(), scenarioPoint.date);
        }
      }
      break;
    }
      // TODO Play interval ? the code is already here.
    default:
    {
      auto root = score::IDocument::get<ScenarioDocumentPresenter>(
          m_sm.context().context.document);
      SCORE_ASSERT(root);
      if(auto root_itv = root->displayedIntervalPresenter())
      {
        auto itv_pt = root_itv->view()->mapFromScene(scenePoint);
        auto global_time = TimeVal::fromPixels(itv_pt.x(), root_itv->zoomRatio());
        m_exec.playAtDate(global_time);
      }
      break;
    }
  }
}

void PlayToolState::on_moved() { }

void PlayToolState::on_released() { }
}
