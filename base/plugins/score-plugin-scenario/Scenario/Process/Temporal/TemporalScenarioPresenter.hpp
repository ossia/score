#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <QObject>
#include <QPoint>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Process/Temporal/ScenarioViewInterface.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/tools/std/Optional.hpp>

#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/widgets/GraphicsItem.hpp>

extern template class IdContainer<Scenario::StatePresenter, Scenario::StateModel>;
extern template class IdContainer<Scenario::EventPresenter, Scenario::EventModel>;
extern template class IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel>;
extern template class IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel>;
extern template class IdContainer<Scenario::CommentBlockPresenter, Scenario::CommentBlockModel>;
namespace Scenario
{

class EditionSettings;
class TemporalScenarioView;

class SCORE_PLUGIN_SCENARIO_EXPORT TemporalScenarioPresenter final
    : public Process::LayerPresenter
    , public Nano::Observer
{
  Q_OBJECT

  friend class Scenario::ToolPalette;
  friend class ScenarioViewInterface;
  friend class ScenarioSelectionManager;

public:
  TemporalScenarioPresenter(
      Scenario::EditionSettings&,
      const Scenario::ProcessModel& model,
      Process::LayerView* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent);
  ~TemporalScenarioPresenter();

  const Scenario::ProcessModel& model() const override;
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

  EventPresenter& event(const Id<EventModel>& id) const;
  TimeSyncPresenter& timeSync(const Id<TimeSyncModel>& id) const;
  IntervalPresenter& interval(const Id<IntervalModel>& id) const;
  StatePresenter& state(const Id<StateModel>& id) const;
  const auto& comment(const Id<CommentBlockModel>& id) const
  {
    return m_comments.at(id);
  }
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
  const auto& getComments() const
  {
    return m_comments;
  }

  TemporalScenarioView& view() const
  {
    return *m_view;
  }
  const ZoomRatio& zoomRatio() const
  {
    return m_zoomRatio;
  }

  Scenario::ToolPalette& stateMachine()
  {
    return m_sm;
  }
  Scenario::EditionSettings& editionSettings() const
  {
    return m_editionSettings;
  }

  void fillContextMenu(
      QMenu&,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

  bool event(QEvent* e) override
  {
    return QObject::event(e);
  }

  void drawDragLine(const Scenario::StateModel&, Scenario::Point) const;
  void stopDrawDragLine() const;
Q_SIGNALS:
  void linesExtremityScaled(int, int);

  void keyPressed(int);
  void keyReleased(int);

public:
  // Model -> view
  void on_stateCreated(const StateModel&);
  void on_stateRemoved(const StateModel&);

  void on_eventCreated(const EventModel&);
  void on_eventRemoved(const EventModel&);

  void on_timeSyncCreated(const TimeSyncModel&);
  void on_timeSyncRemoved(const TimeSyncModel&);

  void on_intervalCreated(const IntervalModel&);
  void on_intervalRemoved(const IntervalModel&);

  void on_commentCreated(const CommentBlockModel&);
  void on_commentRemoved(const CommentBlockModel&);

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
  void eventHasTrigger(const EventPresenter&, bool);

  ZoomRatio m_zoomRatio{1};
  double m_graphicalScale{1.};

  const Scenario::ProcessModel& m_layer;

  // The order of deletion matters!
  // m_view has to be deleted after the other elements.
  graphics_item_ptr<TemporalScenarioView> m_view;

  IdContainer<StatePresenter, StateModel> m_states;
  IdContainer<EventPresenter, EventModel> m_events;
  IdContainer<TimeSyncPresenter, TimeSyncModel> m_timeSyncs;
  IdContainer<TemporalIntervalPresenter, IntervalModel> m_intervals;
  IdContainer<CommentBlockPresenter, CommentBlockModel> m_comments;

  ScenarioViewInterface m_viewInterface;

  Scenario::EditionSettings& m_editionSettings;

  OngoingCommandDispatcher m_ongoingDispatcher;

  score::SelectionDispatcher m_selectionDispatcher;
  Scenario::ToolPalette m_sm;

  QMetaObject::Connection m_con;
};
}
