#pragma once
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPoint>
#include <QRect>

#include <score_plugin_curve_export.h>
#include <verdigris>
class QGraphicsSceneContextMenuEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
class SegmentModel;

struct Style;
class SCORE_PLUGIN_CURVE_EXPORT SegmentView final : public QObject,
                                                    public QGraphicsItem
{
  W_OBJECT(SegmentView)
  Q_INTERFACES(QGraphicsItem)
public:
  SegmentView(
      const SegmentModel* model,
      const Curve::Style& style,
      QGraphicsItem* parent);

  const Id<SegmentModel>& id() const;

  static const constexpr int Type = QGraphicsItem::UserType + 101;
  int type() const final override { return Type; }

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  QPainterPath opaqueArea() const override;
  bool contains(const QPointF& pt) const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setModel(const SegmentModel*);
  const SegmentModel& model() const { return *m_model; }

  void setRect(const QRectF& theRect);

  void setSelected(bool selected);

  void enable();
  void disable();

  void setTween(bool b);

public:
  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, contextMenuRequested, arg_1, arg_2)

protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
  void recomputeStroke() const;
  void updatePoints();
  void updatePen();
  // Takes a table of points and draws them in a square given by the
  // boundingRect
  // QGraphicsItem interface
  QRectF m_rect;

  const SegmentModel* m_model{};
  const QPen* m_pen{};
  const Curve::Style& m_style;

  QPainterPath m_unstrokedShape;
  mutable QPainterPath m_strokedShape;

  bool m_enabled{true};
  bool m_tween{false};
  bool m_selected{};
  mutable bool m_needsRecompute{false};
};
}

#if !defined(SCORE_ALL_UNITY)
extern template class IdContainer<Curve::SegmentView, Curve::SegmentModel>;
#endif
