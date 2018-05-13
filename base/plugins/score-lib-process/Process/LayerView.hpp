#pragma once
#include <QGraphicsItem>
#include <wobjectdefs.h>
#include <QGraphicsSceneDragDropEvent>
#include <QRect>
#include <QtGlobal>
#include <score/widgets/MimeData.hpp>
#include <score_lib_process_export.h>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QMimeData;
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT LayerView
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(LayerView)
  Q_INTERFACES(QGraphicsItem)
public:
  LayerView(QGraphicsItem* parent);

  virtual ~LayerView() override;

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

  void setHeight(qreal height);
  qreal height() const;

  void setWidth(qreal width);
  qreal width() const;

  virtual QPixmap pixmap();

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;

public:
  void heightChanged() W_SIGNAL(heightChanged);
  void widthChanged() W_SIGNAL(widthChanged);

  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);
  void released(QPointF arg_1) W_SIGNAL(released, arg_1);
  void moved(QPointF arg_1) W_SIGNAL(moved, arg_1);
  void doubleClicked(QPointF arg_1) W_SIGNAL(doubleClicked, arg_1);

  // Screen pos, scene pos
  void askContextMenu(const QPoint& arg_1, const QPointF& arg_2) W_SIGNAL(askContextMenu, arg_1, arg_2);
  void dropReceived(const QPointF& pos, const QMimeData& arg_2) W_SIGNAL(dropReceived, pos, arg_2);

protected:
  virtual void paint_impl(QPainter*) const = 0;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  qreal m_height{};
  qreal m_width{};
};

class SCORE_LIB_PROCESS_EXPORT MiniLayer : public QGraphicsItem
{
  Q_INTERFACES(QGraphicsItem)
public:
  MiniLayer(QGraphicsItem* parent);

  virtual ~MiniLayer();

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

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
