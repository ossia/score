#pragma once
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <QPoint>
#include <QString>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
class QGraphicsItem;
class QMimeData;
class QObject;
#include <score/model/Identifier.hpp>

namespace Scenario
{
class EventModel;
class EventView;
class SCORE_PLUGIN_SCENARIO_EXPORT EventPresenter final : public QObject
{
  W_OBJECT(EventPresenter)

public:
  EventPresenter(
      const EventModel& model,
      QGraphicsItem* parentview,
      QObject* parent);
  virtual ~EventPresenter();

  const Id<EventModel>& id() const;

  EventView* view() const;
  const EventModel& model() const;

  bool isSelected() const;
  void handleDrop(const QPointF& pos, const QMimeData& mime);

public:
  void pressed(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  void eventHoverEnter()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventHoverEnter)
  void eventHoverLeave()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventHoverLeave)

private:
  const EventModel& m_model;
  graphics_item_ptr<EventView> m_view;
};
}
