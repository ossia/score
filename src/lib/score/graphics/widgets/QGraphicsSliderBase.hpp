#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{

template <typename T>
struct SCORE_LIB_BASE_EXPORT QGraphicsSliderBase : public QGraphicsItem
{
    QGraphicsSliderBase(QGraphicsItem* parent);
    ~QGraphicsSliderBase();

    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;

    void setRect(const QRectF& r);
    QRectF boundingRect() const override;

    QRectF m_rect{defaultSliderSize};
    struct RightClickImpl;
    RightClickImpl* impl{};
};


}
