// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayToolState.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <QApplication>
namespace Scenario
{
static constexpr double g_play_tool_max_speed = 50.0;
static constexpr double g_play_tool_speed_smoothing = 0.75;
static constexpr double g_play_tool_position_correction = 0.85;
PlayToolState::PlayToolState(const Scenario::ToolPalette& sm)
    : m_sm{sm}
    , m_exec{m_sm.context()
                 .context.app.guiApplicationPlugin<ScenarioApplicationPlugin>()
                 .execution()}
{
}

void PlayToolState::on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint)
{
  m_pressedItem = m_sm.scene().itemAt(scenePoint, QTransform());
  if(!m_pressedItem)
    return;

  switch(m_pressedItem->type())
  {
    case StateView::Type: {
      const auto& state
          = safe_cast<const StateView*>(m_pressedItem)->presenter().model();

      auto id = state.parent() == &this->m_sm.model() ? state.id()
                                                      : OptionalId<StateModel>{};
      if(id)
        m_exec.playState(&m_sm.model(), *id);
      break;
    }
    case IntervalHeader::Type:
      m_pressedItem = safe_cast<const IntervalHeader*>(m_pressedItem)->intervalView();
      [[fallthrough]];
    case ItemType::GraphInterval:
    case TemporalIntervalView::Type:
    case FullViewIntervalView::Type: {
      const auto& cst
          = safe_cast<const IntervalView*>(m_pressedItem)->presenter().model();

      if(cst.parent() == &this->m_sm.model())
      {
        if(QApplication::keyboardModifiers() & Qt::AltModifier)
        {
          m_exec.playInterval(const_cast<IntervalModel*>(&cst));
        }
        else
        {
          m_exec.playFromIntervalAtDate(&m_sm.model(), cst.id(), scenarioPoint.date);
        }
      }
      break;
    }
    default: {
      auto root = score::IDocument::get<ScenarioDocumentPresenter>(
          m_sm.context().context.document);
      SCORE_ASSERT(root);
      if(auto root_itv = root->displayedIntervalPresenter())
      {
        auto itv_pt = root_itv->view()->mapFromScene(scenePoint);
        auto global_time = TimeVal::fromPixels(itv_pt.x(), root_itv->zoomRatio());

        m_previousPoint = global_time;
        m_targetPosition = global_time;
        m_previousSpeed = root_itv->model().duration.speed();
        m_speedChanged = false;
        m_maxSpeed = m_previousSpeed;
        m_exec.playAtDate(global_time);
      }
      break;
    }
  }
}

void PlayToolState::on_scrub(QPointF scenePoint, Scenario::Point scenarioPoint)
{
  auto root = score::IDocument::get<ScenarioDocumentPresenter>(
      m_sm.context().context.document);
  SCORE_ASSERT(root);
  if(auto root_itv = root->displayedIntervalPresenter())
  {
    // Timing info
    auto& durations = const_cast<IntervalDurations&>(root_itv->model().duration);
    const auto current_playback_pos
        = durations.defaultDuration() * durations.playPercentage();
    const auto itv_pt = root_itv->view()->mapFromScene(scenePoint);
    const auto mouse_cursor_time
        = TimeVal::fromPixels(itv_pt.x(), root_itv->zoomRatio());

    // Calculate position error and desired speed
    const auto position_error = (mouse_cursor_time - current_playback_pos).msec();
    const auto target_speed = g_play_tool_position_correction * position_error / 1000.0;

    if(m_speedChanged)
    {
      if((std::signbit(target_speed) != std::signbit(m_smoothedSpeed)))
      {
        m_smoothedSpeed = target_speed;
      }
      else
      {
        m_smoothedSpeed = g_play_tool_speed_smoothing * m_smoothedSpeed
                          + (1.0 - g_play_tool_speed_smoothing) * target_speed;
      }
    }
    else
    {
      m_smoothedSpeed = target_speed;
    }

    auto final_speed
        = std::clamp(m_smoothedSpeed, -g_play_tool_max_speed, g_play_tool_max_speed);

    durations.setSpeed(final_speed);
    m_maxSpeed = std::max(final_speed, m_maxSpeed);
    m_speedChanged = true;
    m_previousPoint = mouse_cursor_time;
    m_targetPosition = mouse_cursor_time;
    m_scrubbingTimer.setInterval(100);
    m_scrubbingTimer.stop();
    QObject::disconnect(&m_scrubbingTimer, nullptr, &m_sm, nullptr);
    QObject::connect(&m_scrubbingTimer, &QTimer::timeout, &m_sm, [this, root_itv] {
      m_scrubbingTimer.setInterval(16);

      auto& durations = const_cast<IntervalDurations&>(root_itv->model().duration);

      const auto current_playback_pos
          = durations.playPercentage() * durations.defaultDuration().msec();
      const auto target_pos = m_targetPosition.msec();

      const auto position_error = target_pos - current_playback_pos;
      if(std::abs(position_error) < 10.0)
      {
        durations.setSpeed(0.0);
        m_scrubbingTimer.stop();
        return;
      }
      else if(std::abs(position_error) > 100.)
      {
        if(std::signbit(m_smoothedSpeed) == std::signbit(position_error))
          return;
      }
      const auto target_speed
          = g_play_tool_position_correction * position_error / 1000.0;

      m_smoothedSpeed = g_play_tool_speed_smoothing * m_smoothedSpeed
                        + (1.0 - g_play_tool_speed_smoothing) * target_speed;

      auto final_speed
          = std::clamp(m_smoothedSpeed, -g_play_tool_max_speed, g_play_tool_max_speed);
      durations.setSpeed(final_speed);
    });
    m_scrubbingTimer.start();
  }
}

void PlayToolState::on_moved(QPointF scenePoint, Scenario::Point scenarioPoint)
{
  if(!m_pressedItem)
    return;

  switch(m_pressedItem->type())
  {
    case ItemType::GraphInterval:
    case TemporalIntervalView::Type:
    case FullViewIntervalView::Type: {
      const auto& cst
          = safe_cast<const IntervalView*>(m_pressedItem)->presenter().model();

      if(cst.parent() == &this->m_sm.model())
      {
        if(QApplication::keyboardModifiers() & Qt::AltModifier)
        {
          m_exec.playInterval(const_cast<IntervalModel*>(&cst));
        }
        else
        {
          m_exec.playFromIntervalAtDate(&m_sm.model(), cst.id(), scenarioPoint.date);
        }
      }
      break;
    }
      // TODO Play interval ? the code is already here.
    default: {
      auto root = score::IDocument::get<ScenarioDocumentPresenter>(
          m_sm.context().context.document);
      SCORE_ASSERT(root);
      on_scrub(scenePoint, scenarioPoint);
      break;
    }
  }
}

void PlayToolState::on_released(QPointF scenePoint, Scenario::Point scenarioPoint)
{

  switch(m_pressedItem->type())
  {
    case ItemType::GraphInterval:
    case TemporalIntervalView::Type:
    case FullViewIntervalView::Type:
      break;
    default: {
      auto root = score::IDocument::get<ScenarioDocumentPresenter>(
          m_sm.context().context.document);
      SCORE_ASSERT(root);

      if(m_speedChanged)
      {
        if(auto root_itv = root->displayedIntervalPresenter())
        {
          auto& durations = const_cast<IntervalDurations&>(root_itv->model().duration);
          durations.setSpeed(m_previousSpeed);
          const auto itv_pt = root_itv->view()->mapFromScene(scenePoint);
          const auto global_time
              = TimeVal::fromPixels(itv_pt.x(), root_itv->zoomRatio());

          const auto current_playback_pos
              = durations.playPercentage() * durations.guiDuration().msec();
          if(std::abs(global_time.msec() - current_playback_pos) > 10)
            m_exec.playAtDate(global_time);
        }
      }
      break;
    }
  }
  m_scrubbingTimer.stop();
}
}
