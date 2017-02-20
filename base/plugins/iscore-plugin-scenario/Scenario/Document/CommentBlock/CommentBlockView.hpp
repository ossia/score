#pragma once
#include <iscore/tools/GraphicsItem.hpp>
#include <QObject>

#include <QPointF>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
namespace Scenario
{
class TextItem;

class CommentBlockPresenter;
class CommentBlockView final : public GraphicsItem
{
public:
  CommentBlockView(CommentBlockPresenter& presenter, QQuickPaintedItem* parent);

  //~TimeNodeView() = default;

  static constexpr int static_type()
  {
    return 1337 + ItemType::Comment;
  }
  int type() const override
  {
    return static_type();
  }

  const CommentBlockPresenter& presenter() const
  {
    return m_presenter;
  }

  void paint(
      QPainter* painter) override;

  QRectF boundingRect() const override;

  void setSelected(bool b);
  void setHtmlContent(QString htmlText);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* evt) override;

private:
  void focusOnText();
  void focusOut();

  CommentBlockPresenter& m_presenter;

  TextItem* m_textItem{};
  bool m_selected{false};

  QPointF m_clickedPoint{};
  QPointF m_clickedScenePoint{};
};
}
