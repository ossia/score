#pragma once
#include <score/model/path/ObjectPath.hpp>

#include <QGraphicsItem>
#include <QList>
#include <QRect>

#include <verdigris>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class IntervalModel;
class ClickableLabelItem;
class AddressBarItem final : public QObject, public QGraphicsItem
{
  W_OBJECT(AddressBarItem)
  Q_INTERFACES(QGraphicsItem)
public:
  AddressBarItem(const score::DocumentContext& ctx, QGraphicsItem* parent);

  double width() const;
  void setTargetObject(ObjectPath&&);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

public:
  void needRedraw() W_SIGNAL(needRedraw);
  void intervalSelected(IntervalModel& cst) W_SIGNAL(intervalSelected, cst);

private:
  void redraw();
  QList<QGraphicsItem*> m_items;
  ObjectPath m_currentPath;
  const score::DocumentContext& m_ctx;

  double m_width{};
};
}
