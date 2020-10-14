#pragma once
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>

#include <score/statemachine/CommonSelectionState.hpp>

#include <QPointF>

namespace Scenario
{
class ToolPalette;
class ScenarioPresenter;
template <typename ToolPalette_T, typename View_T>
class SelectionState final : public CommonSelectionState
{
private:
  QPointF m_initialPoint;
  QPointF m_movePoint;
  const ToolPalette_T& m_parentSM;
  View_T& m_scenarioView;

public:
  SelectionState(
      score::SelectionStack& stack,
      const ToolPalette_T& parentSM,
      View_T& scenarioview,
      QState* parent)
      : CommonSelectionState{stack, &scenarioview, parent}
      , m_parentSM{parentSM}
      , m_scenarioView{scenarioview}
  {
  }

  const QPointF& initialPoint() const { return m_initialPoint; }
  const QPointF& movePoint() const { return m_movePoint; }

  void on_pressAreaSelection() override { m_initialPoint = m_parentSM.scenePoint; }

  void on_moveAreaSelection() override
  {
    m_movePoint = m_parentSM.scenePoint;
    auto area
        = QRectF{m_scenarioView.mapFromScene(m_initialPoint), m_scenarioView.mapFromScene(m_movePoint)}
              .normalized();
    m_scenarioView.setSelectionArea(area);
    setSelectionArea(area);
  }

  void on_releaseAreaSelection() override
  {
    if (m_parentSM.scenePoint == m_initialPoint)
    {
      dispatcher.deselect();
      dispatcher.select(m_parentSM.model());
    }

    m_scenarioView.setSelectionArea(QRectF{});
  }

  void on_deselect() override
  {
    dispatcher.deselect();
    m_scenarioView.setSelectionArea(QRectF{});
  }

  void setSelectionArea(const QRectF& area)
  {
    using namespace std;
    Selection sel;

    auto& presenter = m_parentSM.presenter();

    for (const auto& elt : presenter.getIntervals())
    {
      if (area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
      {
        sel.append(elt.model());
      }
    }

    if constexpr (std::is_same_v<
                      std::remove_reference_t<decltype(presenter)>,
                      const Scenario::ScenarioPresenter>)
    {
      for (const auto& elt : presenter.getGraphIntervals())
      {
        if (area.intersects(elt.boundingRect().translated(elt.pos())))
        {
          sel.append(elt.model());
        }
      }
    }

    for (const auto& elt : presenter.getTimeSyncs())
    {
      if (area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
      {
        sel.append(elt.model());
      }
    }
    for (const auto& elt : presenter.getEvents())
    {
      if (area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
      {
        sel.append(elt.model());
      }
    }
    for (const auto& elt : presenter.getStates())
    {
      if (area.intersects(elt.view()->boundingRect().translated(elt.view()->pos())))
      {
        sel.append(elt.model());
      }
    }

    dispatcher.select(
        filterSelections(sel, m_parentSM.model().selectedChildren(), multiSelection()));
  }
};
}
