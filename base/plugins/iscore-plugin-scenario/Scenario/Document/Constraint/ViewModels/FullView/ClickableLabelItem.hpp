#pragma once
#include <QQuickPaintedItem>
#include <QString>
#include <functional>
namespace iscore
{
class ModelMetadata;
}
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{
class SeparatorItem final : public QGraphicsSimpleTextItem
{
public:
  SeparatorItem(QQuickPaintedItem* parent);
};

class ClickableLabelItem final : public QObject, public QGraphicsSimpleTextItem
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
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  int m_index{-1};
  ClickHandler m_onClick;
};
}
