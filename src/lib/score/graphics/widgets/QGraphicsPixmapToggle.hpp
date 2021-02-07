#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle final : public QObject,
        public QGraphicsPixmapItem
{
    W_OBJECT(QGraphicsPixmapToggle)
    Q_INTERFACES(QGraphicsItem)

    const QPixmap m_pressed, m_released;
    bool m_toggled{};

public:
    QGraphicsPixmapToggle(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

    void toggle();
    void setState(bool toggled);

public:
    void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};
}
