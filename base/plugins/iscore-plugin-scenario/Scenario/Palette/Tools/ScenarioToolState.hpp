#pragma once
#include <QQuickPaintedItem>
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

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>

#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/RightBrace.hpp>

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
  OptionalId<EventModel> itemToEventId(const QQuickPaintedItem* pressedItem) const
  {
    const auto& event
        = static_cast<const EventView*>(pressedItem)->presenter().model();
    return event.parent() == &this->m_palette.model()
               ? event.id()
               : OptionalId<EventModel>{};
  }
  OptionalId<TimeNodeModel>
  itemToTimeNodeId(const QQuickPaintedItem* pressedItem) const
  {
    const auto& timenode
        = static_cast<const TimeNodeView*>(pressedItem)->presenter().model();
    return timenode.parent() == &this->m_palette.model()
               ? timenode.id()
               : OptionalId<TimeNodeModel>{};
  }
  OptionalId<ConstraintModel>
  itemToConstraintId(const QQuickPaintedItem* pressedItem) const
  {
    const auto& constraint = static_cast<const ConstraintView*>(pressedItem)
                                 ->presenter()
                                 .abstractConstraintViewModel()
                                 .model();
    return constraint.parent() == &this->m_palette.model()
               ? constraint.id()
               : OptionalId<ConstraintModel>{};
  }
  OptionalId<StateModel> itemToStateId(const QQuickPaintedItem* pressedItem) const
  {
    const auto& state
        = static_cast<const StateView*>(pressedItem)->presenter().model();

    return state.parent() == &this->m_palette.model()
               ? state.id()
               : OptionalId<StateModel>{};
  }
  const SlotModel* itemToSlotFromHandle(const QQuickPaintedItem* pressedItem) const
  {
    const auto& slot = static_cast<const SlotHandle*>(pressedItem)
                           ->slotView()
                           .presenter.model();

    return slot.parentConstraint().parent() == &this->m_palette.model()
               ? &slot
               : nullptr;
  }

  template <
      typename EventFun,
      typename StateFun,
      typename TimeNodeFun,
      typename ConstraintFun,
      typename LeftBraceFun,
      typename RightBraceFun,
      typename SlotHandleFun,
      typename NothingFun>
  void mapTopItem(
      const QQuickPaintedItem* item,
      StateFun st_fun,
      EventFun ev_fun,
      TimeNodeFun tn_fun,
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
    // Check if it is an event / timenode / constraint /state
    // The itemToXXXId methods check that we are in the correct scenario, too.
    switch (item->type())
    {
      case EventView::static_type():
        tryFun(ev_fun, itemToEventId(item));
        break;

      case ConstraintView::static_type():
        tryFun(cst_fun, itemToConstraintId(item));
        break;

      case TimeNodeView::static_type():
        tryFun(tn_fun, itemToTimeNodeId(item));
        break;

      case StateView::static_type():
        tryFun(st_fun, itemToStateId(item));
        break;

      case SlotHandle::static_type(): // Slot handle
      {
        auto slot = itemToSlotFromHandle(item);
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
