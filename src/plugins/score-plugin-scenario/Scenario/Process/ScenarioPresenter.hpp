#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/PresenterInstantiations.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioViewInterface.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <verdigris>

namespace Scenario
{

class EditionSettings;
class ScenarioView;

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioPresenter final
    : public Process::LayerPresenter,
      public Nano::Observer
{
  W_OBJECT(ScenarioPresenter)

  friend class Scenario::ToolPalette;
  friend class ScenarioViewInterface;
  friend class ScenarioSelectionManager;

public:
  ScenarioPresenter(
      Scenario::EditionSettings&,
      const Scenario::ProcessModel& model,
      Process::LayerView* view,
      const Process::Context& context,
      QObject* parent);
  ~ScenarioPresenter();

  const Scenario::ProcessModel& model() const noexcept;

  /**
   * @brief toScenarioPoint
   *
   * Maps a point in item coordinates
   * to a point in scenario model coordinates (time; y percentage)
   */
  Scenario::Point toScenarioPoint(QPointF pt) const noexcept;
  QPointF fromScenarioPoint(const Scenario::Point& pt) const noexcept;

  void setWidth(qreal width, qreal defaultWidth) override;
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
  const auto& getEvents() const { return m_events; }
  const auto& getTimeSyncs() const { return m_timeSyncs; }
  const auto& getIntervals() const { return m_intervals; }
  const auto& getStates() const { return m_states; }
  const auto& getComments() const { return m_comments; }

  ScenarioView& view() const { return *m_view; }
  const ZoomRatio& zoomRatio() const { return m_zoomRatio; }

  Scenario::ToolPalette& stateMachine() { return m_sm; }
  Scenario::EditionSettings& editionSettings() const
  {
    return m_editionSettings;
  }

  void fillContextMenu(
      QMenu&,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

  bool event(QEvent* e) override { return QObject::event(e); }

  void drawDragLine(const Scenario::StateModel&, Scenario::Point) const;
  void stopDrawDragLine() const;

public:
  void linesExtremityScaled(int arg_1, int arg_2) E_SIGNAL(
      SCORE_PLUGIN_SCENARIO_EXPORT,
      linesExtremityScaled,
      arg_1,
      arg_2)

  void keyPressed(int arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, keyPressed, arg_1)
  void keyReleased(int arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, keyReleased, arg_1)

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

  void on_intervalExecutionTimer();

private:

  void selectLeft();
  void selectRight();
  void selectUp();
  void selectDown();

  void doubleClick(QPointF);
  void on_focusChanged() override;

  template <typename Map, typename Id>
  void removeElement(Map& map, const Id& id);

  void updateAllElements();
  void eventHasTrigger(const EventPresenter&, bool);

  ZoomRatio m_zoomRatio{1};
  double m_graphicalScale{1.};

  // The order of deletion matters!
  // m_view has to be deleted after the other elements.
  graphics_item_ptr<ScenarioView> m_view;

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

const StateModel*
furthestSelectedState(const Scenario::ProcessModel& scenario);
const EventModel*
furthestSelectedEvent(const Scenario::ScenarioPresenter& scenario);
const TimeSyncModel*
furthestSelectedSync(const Scenario::ScenarioPresenter& scenario);

const StateModel* furthestSelectedStateWithoutFollowingInterval(
    const Scenario::ProcessModel& scenario);

// furthest selected state or event are taken into account
const TimeSyncModel*
furthestHierarchicallySelectedTimeSync(const Scenario::ScenarioPresenter& scenario);
}
