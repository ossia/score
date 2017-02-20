#pragma once
#include <QQuickPaintedItem>
#include <QList>
#include <QRect>

#include <iscore/model/path/ObjectPath.hpp>

class QPainter;

class QWidget;

namespace Scenario
{
class ConstraintModel;
class ClickableLabelItem;
class AddressBarItem final : public QQuickPaintedItem
{
  Q_OBJECT

public:
  AddressBarItem(QQuickPaintedItem* parent);

  double width() const;
  void setTargetObject(ObjectPath&&);

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;

signals:
  void needRedraw();
  void constraintSelected(ConstraintModel& cst);

private:
  void redraw();
  QList<QQuickPaintedItem*> m_items;
  ObjectPath m_currentPath;

  double m_width{};
};
}
