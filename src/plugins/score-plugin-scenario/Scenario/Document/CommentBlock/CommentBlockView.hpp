#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPointF>
namespace score
{
class TextItem;
}

namespace Scenario
{
class CommentBlockPresenter;
class CommentBlockView final : public QObject, public QGraphicsItem
{
public:
  CommentBlockView(CommentBlockPresenter& presenter, QGraphicsItem* parent);

  //~TimeSyncView() = default;

  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::Comment;
  }
  int type() const override { return static_type(); }

  const CommentBlockPresenter& presenter() const { return m_presenter; }

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  QRectF boundingRect() const override;

  void setSelected(bool b);
  void setHtmlContent(QString htmlText);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* evt) override;

private:
  void focusOnText();
  void focusOut();

  CommentBlockPresenter& m_presenter;

  score::TextItem* m_textItem{};
  bool m_selected{false};

  QPointF m_clickedPoint{};
  QPointF m_clickedScenePoint{};
};
}
