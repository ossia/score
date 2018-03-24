#pragma once
#include <QGraphicsItem>
#include <QRect>
#include <QTextLayout>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <ossia/detail/optional.hpp>
#include <QGlyphRun>
#include <qnamespace.h>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TemporalIntervalPresenter;
class RackButton final : public QGraphicsObject
{
    Q_OBJECT
  public:
    RackButton(QGraphicsItem* parent);

    void setUnrolled(bool b);

  Q_SIGNALS:
    void clicked();

  private:
    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    bool m_unroll{false};
};


class TemporalIntervalHeader final : public IntervalHeader
{
  Q_OBJECT
public:
  TemporalIntervalHeader(TemporalIntervalPresenter& pres);

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void updateButtons();
  void enableOverlay(bool b);
  void setFocused(bool b) {
    m_hasFocus = b;
    update();
  }
Q_SIGNALS:
  void doubleClicked();

  void intervalHoverEnter();
  void intervalHoverLeave();
  void dropReceived(const QPointF& pos, const QMimeData*);

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
  void on_textChange() override;
  qreal m_previous_x{};

  QRectF m_textRectCache;
  //ossia::optional<QGlyphRun> m_line;
  QImage m_line;
  RackButton* m_button{};
  TemporalIntervalPresenter& m_presenter;
  bool m_hasFocus{};
};
}
