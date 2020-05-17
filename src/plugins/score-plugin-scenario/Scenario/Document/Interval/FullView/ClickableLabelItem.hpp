#pragma once
#include <score/graphics/TextItem.hpp>

#include <QGraphicsItem>
#include <QString>

#include <functional>
#include <verdigris>

namespace score
{
class ModelMetadata;
}
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{
class SeparatorItem final : public QGraphicsItem
{
public:
  SeparatorItem(QGraphicsItem* parent);

  // QGraphicsItem interface
public:
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class ClickableLabelItem final : // public QObject,
                                 public score::SimpleTextItem
{
  W_OBJECT(ClickableLabelItem)
public:
  using ClickHandler = std::function<void(ClickableLabelItem*)>;
  ClickableLabelItem(
      score::ModelMetadata& interval,
      ClickHandler&& onClick,
      const QString& text,
      QGraphicsItem* parent);

  int index() const;
  void setIndex(int index);

public:
  void textChanged() W_SIGNAL(textChanged);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  int m_index{-1};
  ClickHandler m_onClick;
};
}
