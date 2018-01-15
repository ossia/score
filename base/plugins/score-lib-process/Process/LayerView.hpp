#pragma once
#include <QGraphicsItem>
#include <QRect>
#include <QtGlobal>
#include <score_lib_process_export.h>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QMimeData;
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT LayerView : public QObject,
                                            public QGraphicsItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)
public:
  LayerView(QGraphicsItem* parent);

  virtual ~LayerView();

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

  void setHeight(qreal height);
  qreal height() const;

  void setWidth(qreal width);
  qreal width() const;

Q_SIGNALS:
  void heightChanged();
  void widthChanged();

  void pressed(QPointF);
  void released(QPointF);
  void moved(QPointF);
  void doubleClicked(QPointF);

  // Screen pos, scene pos
  void askContextMenu(const QPoint&, const QPointF&);
  void dropReceived(const QPointF& pos, const QMimeData&);

protected:
  virtual void paint_impl(QPainter*) const = 0;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  qreal m_height{};
  qreal m_width{};
};

class SCORE_LIB_PROCESS_EXPORT MiniLayer: public QGraphicsItem
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
