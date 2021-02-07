#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsHSVChooser final : public QObject, public QGraphicsItem
{
    W_OBJECT(QGraphicsHSVChooser)
    Q_INTERFACES(QGraphicsItem)
    QRectF m_rect{0., 0., 140., 100.};

private:
    double h{}, s{}, v{};
    ossia::vec4f m_value{};
    bool m_grab{};

public:
    QGraphicsHSVChooser(QGraphicsItem* parent);

    void setRect(const QRectF& r);
    void setValue(ossia::vec4f v);
    ossia::vec4f value() const;

    bool moving = false;

public:
    void valueChanged(ossia::vec4f arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
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
