#pragma once
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

#include <QGraphicsItem>
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
class PointModel;
struct Style;
class SCORE_PLUGIN_CURVE_EXPORT PointView final : public QObject, public QGraphicsItem
{
  W_OBJECT(PointView)
  Q_INTERFACES(QGraphicsItem)
public:
  PointView(const PointModel* model, const Curve::Style& style, QGraphicsItem* parent);

  const PointModel& model() const;
  const Id<PointModel>& id() const;

  static const constexpr int Type = QGraphicsItem::UserType + 100;
  int type() const final override { return Type; }

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setSelected(bool selected);

  void enable();
  void disable();

  void setModel(const PointModel* model);

public:
  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, contextMenuRequested, arg_1, arg_2)
protected:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

private:
  const PointModel* m_model;
  const Curve::Style& m_style;
  bool m_selected{};
};
}

#if !defined(SCORE_ALL_UNITY)
extern template class IdContainer<Curve::PointView, Curve::PointModel>;
#endif
