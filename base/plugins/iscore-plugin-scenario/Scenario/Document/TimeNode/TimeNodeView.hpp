#pragma once
#include <QColor>
#include <QQuickPaintedItem>
#include <QPoint>
#include <QRect>
#include <QTextLayout>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <iscore/model/ColorReference.hpp>
#include <iscore_plugin_scenario_export.h>

#include <Scenario/Document/CommentBlock/TextItem.hpp>
class QGraphicsSceneMouseEvent;
class QPainter;

class QWidget;

namespace Scenario
{
class TimeNodePresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT TimeNodeView final : public QQuickPaintedItem
{
public:
  TimeNodeView(TimeNodePresenter& presenter, QQuickPaintedItem* parent);
  ~TimeNodeView();

  static constexpr int static_type()
  {
    return 1337 + ItemType::TimeNode;
  }
  int type() const override
  {
    return static_type();
  }

  const TimeNodePresenter& presenter() const
  {
    return m_presenter;
  }

  void paint(
      QPainter* painter) override;

  // QQuickPaintedItem interface
  QRectF boundingRect() const override
  {
    return {-3., 0., 6., m_extent.bottom() - m_extent.top()};
  }

  void setExtent(const VerticalExtent& extent);
  void setExtent(VerticalExtent&& extent);
  void setTriggerActive(bool);
  void addPoint(int newY);

  void setMoving(bool);
  void setSelected(bool selected);

  bool isSelected() const
  {
    return m_selected;
  }

  void changeColor(iscore::ColorRef);
  void setLabel(const QString& label);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  TimeNodePresenter& m_presenter;
  VerticalExtent m_extent;

  QPointF m_clickedPoint{};
  iscore::ColorRef m_color;
  bool m_selected{};

  SimpleTextItem* m_text{};
};
}
