#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QRect>

#include <score/model/path/ObjectPath.hpp>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class IntervalModel;
class ClickableLabelItem;
class AddressBarItem final : public QObject, public QGraphicsItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)
public:
  AddressBarItem(
      const score::DocumentContext& ctx,
      QGraphicsItem* parent);

  double width() const;
  void setTargetObject(ObjectPath&&);

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

Q_SIGNALS:
  void needRedraw();
  void intervalSelected(IntervalModel& cst);

private:
  void redraw();
  QList<QGraphicsItem*> m_items;
  ObjectPath m_currentPath;
  const score::DocumentContext& m_ctx;

  double m_width{};
};
}
