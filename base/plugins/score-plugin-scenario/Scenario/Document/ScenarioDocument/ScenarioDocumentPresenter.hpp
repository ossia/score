#pragma once
#include <QPoint>
#include <QRect>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <memory>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <Dataflow/UI/CableItem.hpp>
#include <score/selection/Selection.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

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
  Q_OBJECT
  friend class DisplayedElementsPresenter;

public:
  ScenarioDocumentPresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view);
  virtual ~ScenarioDocumentPresenter();

  IntervalModel& displayedInterval() const;
  const DisplayedElementsPresenter& presenters() const;
  const ScenarioDocumentModel& model() const;
  ScenarioDocumentView& view() const;
  const Process::ProcessPresenterContext& context() const;
  Process::ProcessFocusManager& focusManager() const;

  // The height in pixels of the displayed interval with its rack.
  // double height() const;
  ZoomRatio zoomRatio() const;


  void selectAll();
  void deselectAll();
  void selectTop();

  void setMillisPerPixel(ZoomRatio newFactor);
  void updateRect(const QRectF& rect);

  void setNewSelection(const Selection& s) override;

  void setDisplayedInterval(Scenario::IntervalModel& interval);

  void on_viewModelDefocused(const Process::ProcessModel* vm);
  void on_viewModelFocused(const Process::ProcessModel* vm);

  DisplayedElementsModel displayedElements;

  IdContainer<Dataflow::CableItem, Process::Cable> cableItems;
Q_SIGNALS:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void escPressed();

  void setFocusedPresenter(QPointer<Process::LayerPresenter>);

private Q_SLOTS:
  void on_windowSizeChanged(QSize);
  void on_viewReady();

private:
  void on_cableAdded(Process::Cable& c);

  void on_cableRemoving(const Process::Cable& c);

  void on_horizontalZoom(QPointF, QPointF);
  void on_verticalZoom(QPointF, QPointF);
  void on_timeRulerScrollEvent(QPointF, QPointF);
  void on_horizontalPositionChanged(int dx);
  void on_minimapChanged(double l, double r);
  ZoomRatio computeZoom(double l, double r);

  void updateMinimap();
  double displayedDuration() const;

  DisplayedElementsPresenter m_scenarioPresenter;

  score::SelectionDispatcher m_selectionDispatcher;
  FocusDispatcher m_focusDispatcher;
  mutable Process::ProcessFocusManager m_focusManager;
  QPointer<IntervalModel> m_focusedInterval{};

  Process::ProcessPresenterContext m_context;

  // State machine
  std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;

  ZoomRatio m_zoomRatio{-1};
  QMetaObject::Connection m_intervalConnection, m_durationConnection;
  Process::MiniLayer* m_miniLayer{};

  bool m_zooming{false};
  bool m_updatingMinimap{false};
  bool m_updatingView{false};

  double computeReverseZoom(ZoomRatio r);
};
}
