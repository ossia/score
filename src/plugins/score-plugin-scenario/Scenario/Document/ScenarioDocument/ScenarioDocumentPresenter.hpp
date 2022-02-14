#pragma once
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <QPoint>
#include <QRect>

#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/CentralIntervalDisplay.hpp>
#include <Scenario/Document/ScenarioDocument/CentralNodalDisplay.hpp>

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
namespace Scenario
{
class DisplayedElementsPresenter;
class IntervalModel;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class ScenarioDocumentView;
class TimeRulerPresenter;

using CentralDisplay = std::variant<std::monostate, CentralIntervalDisplay, CentralNodalDisplay>;
/**
 * @brief The ScenarioDocumentPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the interval model of the base element model.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentPresenter final
    : public score::DocumentDelegatePresenter
    , public Nano::Observer
{
  W_OBJECT(ScenarioDocumentPresenter)
  friend class DisplayedElementsPresenter;
  friend class CentralIntervalDisplay;
  friend class CentralNodalDisplay;

public:
  ScenarioDocumentPresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view);
  ~ScenarioDocumentPresenter() override;

  IntervalModel& displayedInterval() const noexcept;
  IntervalPresenter* displayedIntervalPresenter() const noexcept;
  const ScenarioDocumentModel& model() const noexcept;
  ScenarioDocumentView& view() const noexcept;
  const Process::Context& context() const noexcept;
  Process::ProcessFocusManager& focusManager() const noexcept;

  // The height in pixels of the displayed interval with its rack.
  // double height() const;
  ZoomRatio zoomRatio() const noexcept;

  void selectAll();
  void deselectAll();
  void selectTop();

  void setZoomRatio(ZoomRatio newFactor);
  void updateRect(const QRectF& rect);

  void setNewSelection(const Selection& old, const Selection& s) override;

  void setDisplayedInterval(Scenario::IntervalModel* interval);

  void on_viewModelDefocused(const Process::ProcessModel* vm);
  void on_viewModelFocused(const Process::ProcessModel* vm);

  void focusFrontProcess();
  void goUpALevel();

  DisplayedElementsModel displayedElements;

  void setLargeView();

  void startTimeBar();
  void stopTimeBar();

  bool isNodal() const noexcept;

  void setAutoScroll(bool);

public:
  void pressed(QPointF arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(QPointF arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(QPointF arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)
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
  void updateMinimap();
  double computeReverseZoom(ZoomRatio r);
  void switchMode(bool nodal);
  void recenterNodal();
  void restoreZoom();
  void autoScroll();

  void on_cableAdded(Process::Cable& c);

  void on_cableRemoving(const Process::Cable& c);

  void on_timeRulerChanged();
  void on_horizontalZoom(QPointF, QPointF);
  void on_verticalZoom(QPointF, QPointF);
  void on_timeRulerScrollEvent(QPointF, QPointF);
  void on_visibleRectChanged(const QRectF& c);
  void on_horizontalPositionChanged(int dx);
  void on_minimapChanged(double l, double r);
  void on_executionTimer();
  void on_timelineModeSwitch(bool b);
  ZoomRatio computeZoom(double l, double r);

  void on_addProcessFromLibrary(const Library::ProcessData& dat);

  Process::DataflowManager m_dataflow;
  CentralDisplay m_centralDisplay;
  score::SelectionDispatcher m_selectionDispatcher;
  FocusDispatcher m_focusDispatcher;
  mutable Process::ProcessFocusManager m_focusManager;
  QPointer<IntervalModel> m_focusedInterval{};

  Process::Context m_context;

  ZoomRatio m_zoomRatio{-1};
  QMetaObject::Connection m_intervalConnection, m_durationConnection;
  Process::MiniLayer* m_miniLayer{};

  QAction* m_timelineAction{};
  QAction* m_musicalAction{};

  int m_nonGLTimebarTimer{-1};

  bool m_zooming{false};
  bool m_updatingMinimap{false};
  bool m_reloadingMinimap{false};
  bool m_updatingView{false};
  bool m_autoScroll{false};
};

Process::ProcessModel* closestParentProcessBeforeInterval(const QObject* obj);
void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Port& p);
}
