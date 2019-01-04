#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <Sequence/SequenceModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Instantiations.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <wobjectdefs.h>

namespace Sequence
{

class EditionSettings;
class SequenceView;

class SequencePresenter;
class ViewUpdater
{
public:
  ViewUpdater(const SequencePresenter& presenter);

  void on_eventMoved(const EventPresenter& event);
  void on_intervalMoved(const TemporalIntervalPresenter& interval);
  void on_timeSyncMoved(const TimeSyncPresenter& timesync);
  void on_stateMoved(const StatePresenter& state);

  void on_hoverOnInterval(const Id<IntervalModel>& intervalId, bool enter);
  void on_hoverOnEvent(const Id<EventModel>& eventId, bool enter);

  void on_graphicalScaleChanged(double scale);

private:
  const SequencePresenter& m_presenter;
};
class SCORE_PLUGIN_SCENARIO_EXPORT SequencePresenter final
    : public Process::LayerPresenter,
      public Nano::Observer
{
  W_OBJECT(SequencePresenter)

  friend class Scenario::ToolPalette;
  friend class ViewUpdater;
  friend class ScenarioSelectionManager;

public:
  SequencePresenter(
      Scenario::EditionSettings&, const Sequence::ProcessModel& model,
      Process::LayerView* view,
      const Process::ProcessPresenterContext& context, QObject* parent);
  ~SequencePresenter();

  const Sequence::ProcessModel& model() const override;
  const Id<Process::ProcessModel>& modelId() const override;

  /**
   * @brief toScenarioPoint
   *
   * Maps a point in item coordinates
   * to a point in scenario model coordinates (time; y percentage)
   */
  Scenario::Point toScenarioPoint(QPointF pt) const;

  void setWidth(qreal width) override;
  void setHeight(qreal height) override;
  void putToFront() override;
  void putBehind() override;

  void parentGeometryChanged() override;

  void on_zoomRatioChanged(ZoomRatio val) override;

  Scenario::EventPresenter& event(const Id<Scenario::EventModel>& id) const;
  Scenario::TimeSyncPresenter& timeSync(const Id<Scenario::TimeSyncModel>& id) const;
  Scenario::IntervalPresenter& interval(const Id<Scenario::IntervalModel>& id) const;
  Scenario::StatePresenter& state(const Id<Scenario::StateModel>& id) const;

  const auto& getEvents() const
  {
    return m_events;
  }
  const auto& getTimeSyncs() const
  {
    return m_timeSyncs;
  }
  const auto& getIntervals() const
  {
    return m_intervals;
  }
  const auto& getStates() const
  {
    return m_states;
  }

  SequenceView& view() const
  {
    return *m_view;
  }
  const ZoomRatio& zoomRatio() const
  {
    return m_zoomRatio;
  }

  void fillContextMenu(
      QMenu&, QPoint pos, QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

  bool event(QEvent* e) override
  {
    return QObject::event(e);
  }

public:
public:
  // Model -> view
  void on_stateCreated(const Scenario::StateModel&);
  void on_stateRemoved(const Scenario::StateModel&);

  void on_eventCreated(const Scenario::EventModel&);
  void on_eventRemoved(const Scenario::EventModel&);

  void on_timeSyncCreated(const Scenario::TimeSyncModel&);
  void on_timeSyncRemoved(const Scenario::TimeSyncModel&);

  void on_intervalCreated(const Scenario::IntervalModel&);
  void on_intervalRemoved(const Scenario::IntervalModel&);

  void on_askUpdate();

  void on_keyPressed(int);
  void on_keyReleased(int);

  void on_intervalExecutionTimer();

private:
  void doubleClick(QPointF);
  void on_focusChanged() override;

  template <typename Map, typename Id>
  void removeElement(Map& map, const Id& id);

  void updateAllElements();
  void eventHasTrigger(const Scenario::EventPresenter&, bool);

  ZoomRatio m_zoomRatio{1};
  double m_graphicalScale{1.};

  const Sequence::ProcessModel& m_layer;

  // The order of deletion matters!
  // m_view has to be deleted after the other elements.
  graphics_item_ptr<SequenceView> m_view;

  IdContainer<Scenario::StatePresenter, Scenario::StateModel> m_states;
  IdContainer<Scenario::EventPresenter, Scenario::EventModel> m_events;
  IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel> m_timeSyncs;
  IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel> m_intervals;

  ViewUpdater m_viewInterface;

  Scenario::EditionSettings& m_editionSettings;

  score::SelectionDispatcher m_selectionDispatcher;

  QMetaObject::Connection m_con;
};
}
