#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsCheckBox final
        : public QObject,
        public QGraphicsItem
{
    W_OBJECT(QGraphicsCheckBox)
    Q_INTERFACES(QGraphicsItem)
    QRectF m_rect{defaultCheckBoxSize};

    bool m_toggled{};

public:
    QGraphicsCheckBox(QGraphicsItem* parent);

    void toggle();
    void setState(bool toggled);

public:
    void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

    private:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
}
