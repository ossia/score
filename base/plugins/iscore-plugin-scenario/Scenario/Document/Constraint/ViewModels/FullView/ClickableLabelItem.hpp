#pragma once
#include <QQuickPaintedItem>
#include <QString>
#include <functional>
namespace iscore
{
class ModelMetadata;
}
class QHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{
class SeparatorItem final : public QQuickPaintedItem
{
public:
  SeparatorItem(QQuickPaintedItem* parent);
  void paint(QPainter *painter) override;
};

class ClickableLabelItem final : public QQuickPaintedItem
{
  Q_OBJECT
public:
  using ClickHandler = std::function<void(ClickableLabelItem*)>;
  ClickableLabelItem(
      iscore::ModelMetadata& constraint,
      ClickHandler&& onClick,
      const QString& text,
      QQuickPaintedItem* parent);

  int index() const;
  void setIndex(int index);

signals:
  void textChanged();

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void hoverEnterEvent(QHoverEvent* event) override;
  void hoverLeaveEvent(QHoverEvent* event) override;

private:
  void paint(QPainter *painter) override;
  int m_index{-1};
  ClickHandler m_onClick;
  QString m_text;
};
}
