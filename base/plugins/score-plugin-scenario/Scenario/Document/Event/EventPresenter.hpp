#pragma once
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QPoint>
#include <QString>
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
  Q_OBJECT

public:
  EventPresenter(
      const EventModel& model, QGraphicsItem* parentview, QObject* parent);
  virtual ~EventPresenter();

  const Id<EventModel>& id() const;

  EventView* view() const;
  const EventModel& model() const;

  bool isSelected() const;
  void handleDrop(const QPointF& pos, const QMimeData* mime);

Q_SIGNALS:
  void pressed(const QPointF&);
  void moved(const QPointF&);
  void released(const QPointF&);

  void eventHoverEnter();
  void eventHoverLeave();

private:
  const EventModel& m_model;
  graphics_item_ptr<EventView> m_view;
};
}
