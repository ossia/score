#pragma once
#include <QGraphicsItem>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{

class SCORE_LIB_BASE_EXPORT ZoomItem
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(ZoomItem)
public:
  ZoomItem(QGraphicsItem* parent);
  ~ZoomItem();

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void zoom() E_SIGNAL(SCORE_LIB_BASE_EXPORT, zoom)
  void dezoom() E_SIGNAL(SCORE_LIB_BASE_EXPORT, dezoom)
  void recenter() E_SIGNAL(SCORE_LIB_BASE_EXPORT, recenter)
};

}
