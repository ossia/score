#pragma once
#include <QPoint>
#include <wobjectdefs.h>
#include <QString>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/widgets/GraphicsItem.hpp>
#include <score_plugin_scenario_export.h>
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
      const EventModel& model, QGraphicsItem* parentview, QObject* parent);
  virtual ~EventPresenter();

  const Id<EventModel>& id() const;

  EventView* view() const;
  const EventModel& model() const;

  bool isSelected() const;
  void handleDrop(const QPointF& pos, const QMimeData& mime);

public:
  void pressed(const QPointF& arg_1) W_SIGNAL(pressed, arg_1);
  void moved(const QPointF& arg_1) W_SIGNAL(moved, arg_1);
  void released(const QPointF& arg_1) W_SIGNAL(released, arg_1);

  void eventHoverEnter() W_SIGNAL(eventHoverEnter);
  void eventHoverLeave() W_SIGNAL(eventHoverLeave);

private:
  const EventModel& m_model;
  graphics_item_ptr<EventView> m_view;
};
}
