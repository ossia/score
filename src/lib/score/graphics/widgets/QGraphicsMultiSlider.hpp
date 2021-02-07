#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{
struct SliderWrapper;
struct RightClickImpl;
class SCORE_LIB_BASE_EXPORT QGraphicsMultiSlider final : public QObject, public QGraphicsItem
{
    W_OBJECT(QGraphicsMultiSlider)
    Q_INTERFACES(QGraphicsItem)
    QRectF m_rect{0., 0., 100., 100.};

private:
    ossia::value m_value{};
    bool m_grab{};

public:
    friend struct SliderWrapper;
    double min{0.}, max{1.};

    QGraphicsMultiSlider(QGraphicsItem* parent);

    void setPoint(const QPointF& r);
    void setValue(ossia::value v);
    ossia::value value() const;

    bool moving = false;
    RightClickImpl* impl{};

public:
    void valueChanged(ossia::value arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
    void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
    void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

    private:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
}
