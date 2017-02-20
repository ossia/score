#pragma once
#include <QBrush>
#include <QQuickPaintedItem>
#include <QPen>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <qnamespace.h>
class QGraphicsSceneMouseEvent;

struct ScenarioStyle;
namespace Scenario
{
class ConstraintPresenter;
class LeftBraceView;
class RightBraceView;
class SimpleTextItem;
class ConstraintMenuOverlay;
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintView : public QQuickPaintedItem
{
  Q_OBJECT


public:
  ConstraintView(ConstraintPresenter& presenter, QQuickPaintedItem* parent);
  virtual ~ConstraintView();

  static constexpr int static_type()
  {
    return 1337 + ItemType::Constraint;
  }
  int type() const final override
  {
    return static_type();
  }

  const ConstraintPresenter& presenter() const
  {
    return m_presenter;
  }

  void setInfinite(bool);
  bool infinite() const
  {
    return m_infinite;
  }

  void setDefaultWidth(double width);
  void setMaxWidth(bool infinite, double max);
  void setMinWidth(double min);
  void setHeight(double height);
  void setPlayWidth(double width);
  void setValid(bool val);

  double height() const
  {
    return m_height;
  }

  void setSelected(bool selected);
  virtual void enableOverlay(bool selected);

  bool isSelected() const
  {
    return m_selected;
  }

  double defaultWidth() const
  {
    return m_defaultWidth;
  }

  double minWidth() const
  {
    return m_minWidth;
  }

  double maxWidth() const
  {
    return m_maxWidth;
  }

  double constraintAndRackHeight() const
  {
    return m_height;
  }

  double playWidth() const
  {
    return m_playWidth;
  }

  bool isValid() const
  {
    return m_validConstraint;
  }

  void setFocused(bool b)
  {
    m_hasFocus = b;
    update();
  }

  bool shadow() const;
  void setShadow(bool shadow);

  bool warning() const;
  void setWarning(bool warning);

  void setExecutionState(ConstraintExecutionState);
  QColor constraintColor(const ScenarioStyle& skin) const;

  void updateLabelPos();
  void updateCounterPos();
  void updateOverlayPos();

  void mousePressEvent(QMouseEvent* event) final override;
  void mouseMoveEvent(QMouseEvent* event) final override;
  void mouseReleaseEvent(QMouseEvent* event) final override;

  LeftBraceView& leftBrace() const
  {
    return *m_leftBrace;
  }
  RightBraceView& rightBrace() const
  {
    return *m_rightBrace;
  }

signals:
  void requestOverlayMenu(QPointF);

protected:
  LeftBraceView* m_leftBrace{};
  RightBraceView* m_rightBrace{};
  SimpleTextItem* m_labelItem{};
  SimpleTextItem* m_counterItem{};

  ConstraintMenuOverlay* m_overlay{};

  ConstraintPresenter& m_presenter;
  double m_defaultWidth{};
  double m_maxWidth{};
  double m_minWidth{};
  double m_playWidth{};

  double m_height{};

  bool m_selected{};
  bool m_infinite{};
  bool m_validConstraint{true};
  bool m_warning{false};
  bool m_shadow{false};
  bool m_hasFocus{};
  ConstraintExecutionState m_state{};
};
}
