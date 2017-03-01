#pragma once
#include <iscore/tools/GraphicsItem.hpp>
#include <QPoint>
#include <QRect>

class QGraphicsSceneContextMenuEvent;
class QPainter;

class QWidget;
#include <iscore/model/Identifier.hpp>

namespace Curve
{
class PointModel;
struct Style;
class PointView final : public GraphicsItem
{
  Q_OBJECT

public:
  PointView(
      const PointModel* model,
      const Curve::Style& style,
      QQuickPaintedItem* parent);

  const PointModel& model() const;
  const Id<PointModel>& id() const;

  static constexpr int static_type()
  {
    return 1337 + 100;
  }
  int type() const override
  {
    return static_type();
  }

  void paint(
      QPainter* painter) override;

  void setSelected(bool selected);

  void enable();
  void disable();

  void setModel(const PointModel* model);

signals:
  void contextMenuRequested(const QPoint&, const QPointF&);

protected:
//  void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

private:
  const PointModel* m_model;
  const Curve::Style& m_style;
  bool m_selected{};
  bool m_enabled{true};
};
}
