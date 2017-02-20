#pragma once
#include <iscore/statemachine/CommonSelectionState.hpp>

#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>

#include <QPointF>

namespace Scenario
{
class ToolPalette;
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
      iscore::SelectionStack& stack,
      const ToolPalette_T& parentSM,
      View_T& scenarioview,
      QState* parent)
      : CommonSelectionState{stack, &scenarioview, parent}
      , m_parentSM{parentSM}
      , m_scenarioView{scenarioview}
  {
  }

  const QPointF& initialPoint() const
  {
    return m_initialPoint;
  }
  const QPointF& movePoint() const
  {
    return m_movePoint;
  }

  void on_pressAreaSelection() override
  {
    m_initialPoint = m_parentSM.scenePoint;
  }

  void on_moveAreaSelection() override
  {
    m_movePoint = m_parentSM.scenePoint;
    auto area = QRectF{m_scenarioView.mapFromScene(m_initialPoint),
                       m_scenarioView.mapFromScene(m_movePoint)}
                    .normalized();
    m_scenarioView.setSelectionArea(area);
    setSelectionArea(area);
  }

  void on_releaseAreaSelection() override
  {
    if (m_parentSM.scenePoint == m_initialPoint)
      on_deselect();

    m_scenarioView.setSelectionArea(QRectF{});
  }

  void on_deselect() override
  {
    dispatcher.setAndCommit(Selection{});
    m_scenarioView.setSelectionArea(QRectF{});
  }

  void on_delete() override
  {
    removeSelection(
        m_parentSM.model(), m_parentSM.context().context.commandStack);
  }

  void on_deleteContent() override
  {
    clearContentFromSelection(
        m_parentSM.model(), m_parentSM.context().context.commandStack);
  }

  void setSelectionArea(const QRectF& area)
  {
    using namespace std;
    Selection sel;

    for (const auto& elt : m_parentSM.presenter().getConstraints())
    {
      if (area.intersects(
              elt.view()->boundingRect().translated(elt.view()->position())))
      {
        sel.append(&elt.model());
      }
    }
    for (const auto& elt : m_parentSM.presenter().getTimeNodes())
    {
      if (area.intersects(
              elt.view()->boundingRect().translated(elt.view()->position())))
      {
        sel.append(&elt.model());
      }
    }
    for (const auto& elt : m_parentSM.presenter().getEvents())
    {
      if (area.intersects(
              elt.view()->boundingRect().translated(elt.view()->position())))
      {
        sel.append(&elt.model());
      }
    }
    for (const auto& elt : m_parentSM.presenter().getStates())
    {
      if (area.intersects(
              elt.view()->boundingRect().translated(elt.view()->position())))
      {
        sel.append(&elt.model());
      }
    }

    dispatcher.setAndCommit(filterSelections(
        sel, m_parentSM.model().selectedChildren(), multiSelection()));
  }
};
}
