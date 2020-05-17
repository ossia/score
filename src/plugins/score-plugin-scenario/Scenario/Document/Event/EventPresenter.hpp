#pragma once
#include <Scenario/Document/VerticalExtent.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <QPoint>

#include <score_plugin_scenario_export.h>

#include <verdigris>
class QGraphicsItem;
class QMimeData;
class QObject;
#include <score/model/Identifier.hpp>

namespace Scenario
{
class EventModel;
class EventView;
class StatePresenter;

class SCORE_PLUGIN_SCENARIO_EXPORT EventPresenter final : public QObject
{
  W_OBJECT(EventPresenter)

public:
  EventPresenter(const EventModel& model, QGraphicsItem* parentview, QObject* parent);
  virtual ~EventPresenter();

  const Id<EventModel>& id() const;

  EventView* view() const;
  const EventModel& model() const;

  bool isSelected() const;
  void handleDrop(const QPointF& pos, const QMimeData& mime);

  void addState(StatePresenter* ev);
  void removeState(StatePresenter* ev);
  const std::vector<StatePresenter*>& states() const noexcept { return m_states; }

  VerticalExtent extent() const noexcept;
  void setExtent(const Scenario::VerticalExtent& extent);

  void extentChanged(const Scenario::VerticalExtent& extent)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, extentChanged, extent)

  void pressed(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  void eventHoverEnter() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventHoverEnter)
  void eventHoverLeave() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventHoverLeave)

private:
  const EventModel& m_model;
  std::vector<StatePresenter*> m_states;
  VerticalExtent m_extent;
  graphics_item_ptr<EventView> m_view;
};
}
