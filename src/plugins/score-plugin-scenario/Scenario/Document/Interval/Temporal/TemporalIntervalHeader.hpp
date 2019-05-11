#pragma once
#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <score/graphics/GraphicWidgets.hpp>
#include <score/widgets/MimeData.hpp>

#include <ossia/detail/optional.hpp>

#include <QGlyphRun>
#include <QGraphicsItem>
#include <QRect>
#include <QTextLayout>
#include <qnamespace.h>

#include <wobjectdefs.h>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TemporalIntervalPresenter;
class RackButton final : public QGraphicsObject
{
  W_OBJECT(RackButton)
public:
  RackButton(QGraphicsItem* parent);

  void setUnrolled(bool b);

public:
  void clicked() W_SIGNAL(clicked);

private:
  QRectF boundingRect() const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  bool m_unroll{false};
};

class TemporalIntervalHeader final : public IntervalHeader
{
  W_OBJECT(TemporalIntervalHeader)
public:
  TemporalIntervalHeader(TemporalIntervalPresenter& pres);

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void updateButtons();
  void enableOverlay(bool b);
  void setSelected(bool b);
  void setLabel(const QString& label);

public:
  void doubleClicked() W_SIGNAL(doubleClicked);

  void intervalHoverEnter() W_SIGNAL(intervalHoverEnter);
  void intervalHoverLeave() W_SIGNAL(intervalHoverLeave);
  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      W_SIGNAL(dropReceived, pos, arg_2);

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
  void setState(IntervalHeader::State s) override;
  void on_textChanged() override;
  qreal m_previous_x{};

  QRectF m_textRectCache;
  QImage m_line;
  RackButton* m_rackButton{};
  score::QGraphicsPixmapToggle* m_mute{};
  score::QGraphicsSlider* m_speed{};
  score::QGraphicsPixmapButton* m_add{};
  TemporalIntervalPresenter& m_presenter;
  bool m_selected : 1;
  bool m_hovered : 1;
};
}
