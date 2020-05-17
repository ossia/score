#pragma once
#include <score/widgets/MimeData.hpp>

#include <QGraphicsItem>
#include <QGraphicsSceneDragDropEvent>
#include <QRect>

#include <score_lib_process_export.h>

#include <verdigris>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QMimeData;
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT LayerView : public QObject, public QGraphicsItem
{
  W_OBJECT(LayerView)
  Q_INTERFACES(QGraphicsItem)
public:
  LayerView(QGraphicsItem* parent);

  virtual ~LayerView() override;

  QRectF boundingRect() const final override;
  void
  paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

  void setHeight(qreal height) noexcept;
  qreal height() const noexcept { return m_height; }

  void setWidth(qreal width) noexcept;
  qreal width() const noexcept { return m_width; }

  virtual QPixmap pixmap() noexcept;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void clearPressed() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, clearPressed)

  void escPressed() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, escPressed)

  void pressed(QPointF arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, pressed, arg_1)
  void released(QPointF arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, released, arg_1)
  void moved(QPointF arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, moved, arg_1)
  void doubleClicked(QPointF arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, doubleClicked, arg_1)

  // Screen pos, scene pos
  void askContextMenu(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, askContextMenu, arg_1, arg_2)
  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, dropReceived, pos, arg_2)
  void presetDropReceived(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetDropReceived, pos, arg_2)

  void keyPressed(int arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, keyPressed, arg_1)
  void keyReleased(int arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, keyReleased, arg_1)

  // Screen pos, scene pos
  void dragEnter(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, dragEnter, pos, arg_2)
  void dragMove(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, dragMove, pos, arg_2)
  void dragLeave(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, dragLeave, pos, arg_2)

protected:
  virtual void paint_impl(QPainter*) const = 0;
  virtual void heightChanged(qreal);
  virtual void widthChanged(qreal);

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
  qreal m_height{};
  qreal m_width{};
  bool m_dropPresetOverlay{false};
};

class SCORE_LIB_PROCESS_EXPORT MiniLayer : public QGraphicsItem
{
  Q_INTERFACES(QGraphicsItem)
public:
  MiniLayer(QGraphicsItem* parent);

  ~MiniLayer() override;

  QRectF boundingRect() const final override;
  void
  paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

  void setHeight(qreal height);
  qreal height() const;

  void setWidth(qreal width);
  qreal width() const;

  void setZoomRatio(qreal);
  qreal zoom() const;

protected:
  virtual void paint_impl(QPainter*) const = 0;

private:
  qreal m_height{};
  qreal m_width{};
  qreal m_zoom{};
};
}
