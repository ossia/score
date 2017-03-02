#pragma once
#include <QPoint>
#include <QRect>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
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
class ScenarioDocumentPresenter final
    : public iscore::DocumentDelegatePresenter
{
  Q_OBJECT
  friend class DisplayedElementsPresenter;

public:
  ScenarioDocumentPresenter(
      iscore::DocumentPresenter* parent_presenter,
      const iscore::DocumentDelegateModel& model,
      iscore::DocumentDelegateView& view);
  virtual ~ScenarioDocumentPresenter();

  const ConstraintModel& displayedConstraint() const;
  const DisplayedElementsPresenter& presenters() const
  {
    return *m_scenarioPresenter;
  }

  const ScenarioDocumentModel& model() const;
  ScenarioDocumentView& view() const;

  // The height in pixels of the displayed constraint with its rack.
  // double height() const;
  ZoomRatio zoomRatio() const;

  void on_askUpdate();

  void selectAll();
  void deselectAll();
  void selectTop();

  void setMillisPerPixel(ZoomRatio newFactor);

  void on_newSelection(const Selection&);

  void updateRect(const QRectF& rect);

  const Process::ProcessPresenterContext& context() const
  {
    return m_context;
  }

signals:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void escPressed();

  void requestDisplayedConstraintChange(ConstraintModel&);

private slots:
  void on_windowSizeChanged(QSize);
  void on_viewSizeChanged(QSize);
private:
  void on_displayedConstraintChanged();
  void on_zoomSliderChanged(double);
  void on_zoomOnWheelEvent(QPointF, QPointF);
  void on_timeRulerScrollEvent(QPointF, QPointF);
  void on_horizontalPositionChanged(int dx);
  void on_elementsScaleChanged(double s);

  // void updateGrid();
  void updateZoom(ZoomRatio newZoom, QPointF focus);

  DisplayedElementsPresenter* m_scenarioPresenter{};

  iscore::SelectionDispatcher m_selectionDispatcher;
  FocusDispatcher m_focusDispatcher;
  Process::ProcessPresenterContext m_context;

  // State machine
  std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;

  // Various widgets
  TimeRulerPresenter* m_mainTimeRuler{};

  ZoomRatio m_zoomRatio;
};
}
