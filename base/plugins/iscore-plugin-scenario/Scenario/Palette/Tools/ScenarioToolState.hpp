#pragma once
#include <QGraphicsItem>
#include <QState>
#include <QStateMachine>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <chrono>
#include <iscore/statemachine/GraphicsSceneTool.hpp>
#include <iscore/model/Identifier.hpp>

#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Event/ConditionView.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>

#include <Scenario/Document/Constraint/SlotHandle.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintView.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintHeader.hpp>
#include <Scenario/Document/Constraint/Temporal/Braces/LeftBrace.hpp>

namespace iscore
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
class ToolBase : public GraphicsSceneTool<Scenario::Point>
{
public:
  ToolBase(const ToolPalette_T& palette)
      : GraphicsSceneTool<Scenario::Point>{palette.scene()}, m_palette{palette}
  {
  }

protected:
  OptionalId<EventModel> itemToEventId(const QGraphicsItem* pressedItem) const
  {
    const auto& event
        = static_cast<const EventView*>(pressedItem)->presenter().model();
    return event.parent() == &this->m_palette.model()
               ? event.id()
               : OptionalId<EventModel>{};
  }
  OptionalId<EventModel> itemToConditionId(const QGraphicsItem* pressedItem) const
  {
      const auto& event
              = static_cast<const EventView*>(pressedItem->parentItem())->presenter().model();
      return event.parent() == &this->m_palette.model()
              ? event.id()
              : OptionalId<EventModel>{};
  }
  OptionalId<TimeSyncModel>
  itemToTimeSyncId(const QGraphicsItem* pressedItem) const
  {
    const auto& timesync
        = static_cast<const TimeSyncView*>(pressedItem)->presenter().model();
    return timesync.parent() == &this->m_palette.model()
               ? timesync.id()
               : OptionalId<TimeSyncModel>{};
  }
  OptionalId<TimeSyncModel>
  itemToTriggerId(const QGraphicsItem* pressedItem) const
  {
      const auto& timesync
              = static_cast<const TimeSyncView*>(pressedItem->parentItem())->presenter().model();
      return timesync.parent() == &this->m_palette.model()
              ? timesync.id()
              : OptionalId<TimeSyncModel>{};
  }
  OptionalId<ConstraintModel>
  itemToConstraintId(const QGraphicsItem* pressedItem) const
  {
    const auto& constraint = static_cast<const ConstraintView*>(pressedItem)
                                 ->presenter()
                                 .model();
    return constraint.parent() == &this->m_palette.model()
               ? constraint.id()
               : OptionalId<ConstraintModel>{};
  }
  OptionalId<StateModel> itemToStateId(const QGraphicsItem* pressedItem) const
  {
    const auto& state
        = static_cast<const StateView*>(pressedItem)->presenter().model();

    return state.parent() == &this->m_palette.model()
               ? state.id()
               : OptionalId<StateModel>{};
  }
  optional<SlotPath> itemToConstraintFromHandle(const QGraphicsItem* pressedItem) const
  {
    auto handle = static_cast<const SlotHandle*>(pressedItem);
    const auto& cst = handle->presenter().model();

    if(cst.parent() == &this->m_palette.model())
    {
      auto fv = isInFullView(cst) ?
            Slot::FullView : Slot::SmallView;
      return SlotPath{cst, handle->slotIndex(), fv};
    }
    else
    {
      return ossia::none;
    }
  }

  template <
      typename EventFun,
      typename StateFun,
      typename TimeSyncFun,
      typename ConstraintFun,
      typename LeftBraceFun,
      typename RightBraceFun,
      typename SlotHandleFun,
      typename NothingFun>
  void mapTopItem(
      const QGraphicsItem* item,
      StateFun st_fun,
      EventFun ev_fun,
      TimeSyncFun tn_fun,
      ConstraintFun cst_fun,
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
    // Check if it is an event / timesync / constraint /state
    // The itemToXXXId methods check that we are in the correct scenario, too.
    switch (item->type())
    {
      case ConditionView::static_type():
        tryFun(ev_fun, itemToConditionId(item));
        break;
      case EventView::static_type():
        tryFun(ev_fun, itemToEventId(item));
        break;

      case ConstraintView::static_type():
        tryFun(cst_fun, itemToConstraintId(item));
        break;

      case TriggerView::static_type():
        tryFun(tn_fun, itemToTriggerId(item));
        break;
      case TimeSyncView::static_type():
        tryFun(tn_fun, itemToTimeSyncId(item));
        break;

      case StateView::static_type():
        tryFun(st_fun, itemToStateId(item));
        break;

      case SlotHandle::static_type(): // Slot handle
      {
        auto slot = itemToConstraintFromHandle(item);
        if (slot)
        {
          handle_fun(*slot);
        }
        else
        {
          nothing_fun();
        }
        break;
      }

      case ConstraintHeader::static_type(): // Constraint header
      {
        tryFun(cst_fun, itemToConstraintId(item->parentItem()));
        break;
      }

      case LeftBraceView::static_type(): // Constraint Left Brace
      {
        tryFun(lbrace_fun, itemToConstraintId(item->parentItem()));
        break;
      }

      case RightBraceView::static_type(): // Constraint Right Brace
      {
        tryFun(rbrace_fun, itemToConstraintId(item->parentItem()));
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
