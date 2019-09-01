#pragma once

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>

#include <QPoint>

#include <score_plugin_scenario_export.h>
#include <sys/types.h>
#include <verdigris>

class QGraphicsItem;
class QMimeData;
class QObject;

namespace Scenario
{
class EventModel;
class TimeSyncModel;
class TimeSyncView;
class TriggerView;

class SCORE_PLUGIN_SCENARIO_EXPORT TimeSyncPresenter final : public QObject
{
  W_OBJECT(TimeSyncPresenter)
public:
  TimeSyncPresenter(
      const TimeSyncModel& model,
      QGraphicsItem* parentview,
      QObject* parent);
  ~TimeSyncPresenter();

  const Id<TimeSyncModel>& id() const;
  int32_t id_val() const { return id().val(); }

  const TimeSyncModel& model() const;
  TimeSyncView* view() const;

  TriggerView& trigger() const noexcept;
  void on_eventAdded(const Id<EventModel>& eventId);
  void handleDrop(const QPointF& pos, const QMimeData& mime);

public:
  void pressed(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  void eventAdded(
      const Id<EventModel>& eventId,
      const Id<TimeSyncModel>& timeSyncId)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventAdded, eventId, timeSyncId)

private:
  const TimeSyncModel& m_model;
  graphics_item_ptr<TimeSyncView> m_view;
  TriggerView* m_triggerView{};
};
}
