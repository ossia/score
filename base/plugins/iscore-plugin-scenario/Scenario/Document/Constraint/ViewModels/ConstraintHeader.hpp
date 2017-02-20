#pragma once
#include <QQuickPaintedItem>
#include <QString>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

class QGraphicsSceneMouseEvent;

namespace Scenario
{
class ConstraintView;
class ConstraintHeader : public QQuickPaintedItem
{
public:
  enum class State
  {
    Hidden,     // No rack, we show nothing
    RackHidden, // There is at least a hidden rack in the constraint
    RackShown   // There is a rack currently shown
  };

  using QQuickPaintedItem::QQuickPaintedItem;

  static constexpr int static_type()
  {
    return 1337 + ItemType::ConstraintHeader;
  }
  int type() const override
  {
    return static_type();
  }

  void setConstraintView(ConstraintView* view)
  {
    m_view = view;
  }

  static constexpr int headerHeight()
  {
    return ConstraintHeaderHeight;
  }

  void setWidth(double width);
  void setText(const QString& text);

  virtual void setState(State s)
  {
    if (s == m_state)
      return;

    if (m_state == State::Hidden)
      show();
    else if (s == State::Hidden)
      hide();

    m_state = s;
    update();
  }

protected:
  void mousePressEvent(QMouseEvent* event) final override;
  void mouseMoveEvent(QMouseEvent* event) final override;
  void mouseReleaseEvent(QMouseEvent* event) final override;

protected:
  virtual void on_textChange()
  {
  }
  ConstraintView* m_view{};
  State m_state{};
  double m_width{};
  QString m_text;
};
}
