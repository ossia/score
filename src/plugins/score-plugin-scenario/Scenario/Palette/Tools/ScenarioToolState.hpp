#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/ObjectMapper.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/model/Identifier.hpp>
#include <score/statemachine/GraphicsSceneTool.hpp>

#include <QGraphicsItem>

#include <chrono>

namespace score
{
class Command;
}

template <typename Element>
bool isUnderMouse(Element ev, const QPointF& scenePos)
{
  return ev->mapRectToScene(ev->boundingRect()).contains(scenePos);
}

template <typename PresenterContainer, typename IdToIgnore>
QList<Id<typename PresenterContainer::model_type>> getCollidingModels(
    const PresenterContainer& array,
    const QVector<IdToIgnore>& ids,
    QPointF scenePt)
{
  using namespace std;
  QList<Id<typename PresenterContainer::model_type>> colliding;

  for (const auto& elt : array)
  {
    if (!ids.contains(elt.id()) && isUnderMouse(elt.view(), scenePt))
    {
      colliding.push_back(elt.model().id());
    }
  }
  // TODO sort the elements according to their Z pos.

  return colliding;
}
namespace Scenario
{
template <typename ToolPalette_T>
class ToolBase
    : public GraphicsSceneTool<Scenario::Point>
    , public ObjectMapper
{
public:
  ToolBase(const ToolPalette_T& palette)
      : GraphicsSceneTool<Scenario::Point>{palette.scene()}, m_palette{palette}
  {
  }

protected:

  template <
      typename EventFun,
      typename StateFun,
      typename TimeSyncFun,
      typename IntervalFun,
      typename LeftBraceFun,
      typename RightBraceFun,
      typename SlotHandleFun,
      typename NothingFun>
  void mapTopItem(
      const QGraphicsItem* item,
      StateFun st_fun,
      EventFun ev_fun,
      TimeSyncFun tn_fun,
      IntervalFun cst_fun,
      LeftBraceFun lbrace_fun,
      RightBraceFun rbrace_fun,
      SlotHandleFun handle_fun,
      NothingFun nothing_fun) const
  {
    if (!item)
    {
      nothing_fun();
      return;
    }
    auto tryFun = [=](auto fun, const auto& id) {
      if (id)
        fun(*id);
      else
        nothing_fun();
    };

    // Each time :
    // Check if it is an event / timesync / interval /state
    // The itemToXXXId methods check that we are in the correct scenario, too.
    auto parent = &this->m_palette.model();

    switch (item->type())
    {
      case ItemType::Condition:
        tryFun(ev_fun, itemToConditionId(item, parent));
        break;
      case ItemType::Event:
        tryFun(ev_fun, itemToEventId(item, parent));
        break;

      case ItemType::Interval:
        tryFun(cst_fun, itemToIntervalId(item, parent));
        break;
      case ItemType::GraphInterval:
        tryFun(cst_fun, itemToGraphIntervalId(item, parent));
        break;
      case ItemType::IntervalHeader:
        tryFun(cst_fun, itemToIntervalId(item->parentItem(), parent));
        break;
      case ItemType::LeftBrace:
        tryFun(lbrace_fun, itemToIntervalId(item->parentItem(), parent));
        break;
      case ItemType::RightBrace:
        tryFun(rbrace_fun, itemToIntervalId(item->parentItem(), parent));
        break;

      case ItemType::Trigger:
        tryFun(tn_fun, itemToTriggerId(item, parent));
        break;
      case ItemType::TimeSync:
        tryFun(tn_fun, itemToTimeSyncId(item, parent));
        break;

      case ItemType::StateOverlay:
        tryFun(st_fun, itemToStateId(item->parentItem(), parent));
        break;
      case ItemType::State:
        tryFun(st_fun, itemToStateId(item, parent));
        break;

      case ItemType::SlotFooter:
      {
        if (auto slot = itemToIntervalFromFooter(item, parent))
          handle_fun(*slot);
        else
          nothing_fun();
        break;
      }
      case ItemType::SlotFooterDelegate:
      {
        if (auto slot = itemToIntervalFromFooter(item->parentItem(), parent))
          handle_fun(*slot);
        else
          nothing_fun();
        break;
      }

      default:
        nothing_fun();
        break;
    }
  }

  const ToolPalette_T& m_palette;
};
}
