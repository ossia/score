#pragma once
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>

#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <QPoint>
#include <QRect>

#include <memory>
#include <verdigris>

class QAction;
class ObjectPath;
class QSize;
namespace score
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;
} // namespace score

namespace Process
{
class MiniLayer;
}
namespace Library {
struct ProcessData;
}
namespace Scenario
{
class DisplayedElementsPresenter;
class IntervalModel;
class ScenarioDocumentModel;
class ScenarioDocumentView;
class TimeRulerPresenter;

/**
 * @brief The ScenarioDocumentPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the interval model of the base element model.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentPresenter final
    : public score::DocumentDelegatePresenter
{
  W_OBJECT(ScenarioDocumentPresenter)
  friend class DisplayedElementsPresenter;

public:
  ScenarioDocumentPresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view);
  ~ScenarioDocumentPresenter() override;

  IntervalModel& displayedInterval() const;
  DisplayedElementsPresenter& presenters();
  const ScenarioDocumentModel& model() const;
  ScenarioDocumentView& view() const;
  const Process::Context& context() const;
  Process::ProcessFocusManager& focusManager() const;

  // The height in pixels of the displayed interval with its rack.
  // double height() const;
  ZoomRatio zoomRatio() const;

  void selectAll();
  void deselectAll();
  void selectTop();

  void setZoomRatio(ZoomRatio newFactor);
  void updateRect(const QRectF& rect);

  void setNewSelection(const Selection& old, const Selection& s) override;

  void setDisplayedInterval(Scenario::IntervalModel& interval);
  void createDisplayedIntervalPresenter(Scenario::IntervalModel& interval);

  void on_viewModelDefocused(const Process::ProcessModel* vm);
  void on_viewModelFocused(const Process::ProcessModel* vm);

  DisplayedElementsModel displayedElements;

  void setLargeView();

  void startTimeBar(Scenario::IntervalModel& itv);
  void stopTimeBar();

  bool isNodal() const noexcept;

public:
  void pressed(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(QPointF arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)
  void escPressed() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, escPressed)

  void setFocusedPresenter(QPointer<Process::LayerPresenter> arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, setFocusedPresenter, arg_1)

private:
  void on_windowSizeChanged(QSize);
  W_SLOT(on_windowSizeChanged);
  void on_viewReady();
  W_SLOT(on_viewReady);

private:
  void updateTimeBar();
  void on_cableAdded(Process::Cable& c);

  void on_cableRemoving(const Process::Cable& c);

  void on_horizontalZoom(QPointF, QPointF);
  void on_verticalZoom(QPointF, QPointF);
  void on_timeRulerScrollEvent(QPointF, QPointF);
  void on_horizontalPositionChanged(int dx);
  void on_minimapChanged(double l, double r);
  ZoomRatio computeZoom(double l, double r);

  void on_addProcessFromLibrary(const Library::ProcessData& dat);

  void updateMinimap();
  // double displayedDuration() const;

  Process::DataflowManager m_dataflow;
  DisplayedElementsPresenter m_scenarioPresenter;

  score::SelectionDispatcher m_selectionDispatcher;
  FocusDispatcher m_focusDispatcher;
  mutable Process::ProcessFocusManager m_focusManager;
  QPointer<IntervalModel> m_focusedInterval{};

  Process::Context m_context;

  // State machine
  std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;

  ZoomRatio m_zoomRatio{-1};
  QMetaObject::Connection m_intervalConnection, m_durationConnection;
  Process::MiniLayer* m_miniLayer{};

  bool m_zooming{false};
  bool m_updatingMinimap{false};
  bool m_updatingView{false};

  double computeReverseZoom(ZoomRatio r);

  NodalIntervalView* m_nodal{};

  QAction* m_timelineAction{};
  QAction* m_musicalAction{};
  void switchMode(bool nodal);
  void removeDisplayedIntervalPresenter();
  void recenterNodal();

  QMetaObject::Connection m_nodalDrop, m_nodalContextMenu;
};
}
