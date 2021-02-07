#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <QStringList>

#include <verdigris>
#include <array>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsCombo final
        : public QObject
        , public QGraphicsItem
{
    W_OBJECT(QGraphicsCombo)
    Q_INTERFACES(QGraphicsItem)
    friend struct DefaultComboImpl;
    QRectF m_rect{defaultSliderSize};

public:
    QStringList array;

private:
    int m_value{};
    bool m_grab{};

public:
    template <std::size_t N>
    QGraphicsCombo(const std::array<const char*, N>& arr, QGraphicsItem* parent)
        : QGraphicsCombo{parent}
    {
        array.reserve(N);
        for (auto str : arr)
            array.push_back(str);
    }

    QGraphicsCombo(QStringList arr, QGraphicsItem* parent) : QGraphicsCombo{parent}
    {
        array = std::move(arr);
    }

    QGraphicsCombo(QGraphicsItem* parent);

    void setRect(const QRectF& r);
    void setValue(int v);
    int value() const;

    bool moving = false;

public:
    void valueChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
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
