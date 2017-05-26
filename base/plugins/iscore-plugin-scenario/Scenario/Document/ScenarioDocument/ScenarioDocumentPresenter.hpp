#pragma once
#include <QPoint>
#include <QRect>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <memory>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>

class ObjectPath;
class QSize;
namespace iscore
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;
} // namespace iscore

namespace Process
{
class MiniLayer;
}
namespace Scenario
{
class DisplayedElementsPresenter;
class ConstraintModel;
class ScenarioDocumentModel;
class ScenarioDocumentView;
class TimeRulerPresenter;

/**
 * @brief The ScenarioDocumentPresenter class
 *
 * A bit special because we connect it to the presenter of the content model
 * inside the constraint model of the base element model.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentPresenter final
    : public iscore::DocumentDelegatePresenter
{
  Q_OBJECT
  friend class DisplayedElementsPresenter;

public:
  ScenarioDocumentPresenter(
      const iscore::DocumentContext& ctx,
      iscore::DocumentPresenter* parent_presenter,
      const iscore::DocumentDelegateModel& model,
      iscore::DocumentDelegateView& view);
  virtual ~ScenarioDocumentPresenter();

  ConstraintModel& displayedConstraint() const;
  const DisplayedElementsPresenter& presenters() const;
  const ScenarioDocumentModel& model() const;
  ScenarioDocumentView& view() const;
  const Process::ProcessPresenterContext& context() const;
  Process::ProcessFocusManager& focusManager() const;

  // The height in pixels of the displayed constraint with its rack.
  // double height() const;
  ZoomRatio zoomRatio() const;


  void selectAll();
  void deselectAll();
  void selectTop();

  void setMillisPerPixel(ZoomRatio newFactor);
  void updateRect(const QRectF& rect);

  void setNewSelection(const Selection& s) override;

  void setDisplayedConstraint(Scenario::ConstraintModel& constraint);

  void on_viewModelDefocused(const Process::ProcessModel* vm);
  void on_viewModelFocused(const Process::ProcessModel* vm);

  DisplayedElementsModel displayedElements;

signals:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void escPressed();

  void setFocusedPresenter(QPointer<Process::LayerPresenter>);

private slots:
  void on_windowSizeChanged(QSize);
  void on_viewReady();

private:
  void on_zoomOnWheelEvent(QPointF, QPointF);
  void on_timeRulerScrollEvent(QPointF, QPointF);
  void on_horizontalPositionChanged(int dx);
  void on_minimapChanged(double l, double r);
  ZoomRatio computeZoom(double l, double r);

  void updateMinimap();
  double displayedDuration() const;

  DisplayedElementsPresenter* m_scenarioPresenter{};

  iscore::SelectionDispatcher m_selectionDispatcher;
  FocusDispatcher m_focusDispatcher;
  TimeRulerPresenter* m_mainTimeRuler{};
  mutable Process::ProcessFocusManager m_focusManager;
  QPointer<ConstraintModel> m_focusedConstraint{};

  Process::ProcessPresenterContext m_context;

  // State machine
  std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;

  ZoomRatio m_zoomRatio{-1};
  QMetaObject::Connection m_constraintConnection, m_durationConnection;
  Process::MiniLayer* m_miniLayer{};

  bool m_zooming{false};
  bool m_updatingMinimap{false};
  bool m_updatingView{false};

  double computeReverseZoom(ZoomRatio r);
};
}
