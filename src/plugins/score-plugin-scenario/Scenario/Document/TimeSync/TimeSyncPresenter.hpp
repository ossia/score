#pragma once

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
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
class EventPresenter;
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
  void handleDrop(const QPointF& pos, const QMimeData& mime);

  void addEvent(EventPresenter* ev);
  void removeEvent(EventPresenter* ev);

  const VerticalExtent& extent() const noexcept;
  void setExtent(const VerticalExtent& extent);

  void extentChanged(const Scenario::VerticalExtent& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, extentChanged, arg_1)

  const std::vector<EventPresenter*>& events() const noexcept { return m_events; }
public:
  void pressed(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(const QPointF& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

private:
  const TimeSyncModel& m_model;
  std::vector<EventPresenter*> m_events;
  graphics_item_ptr<TimeSyncView> m_view;
  TriggerView* m_triggerView{};
  VerticalExtent m_extent;

};
}
