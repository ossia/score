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
class SCORE_LIB_BASE_EXPORT QGraphicsSelectablePixmapToggle final
    : public QObject,
        public QGraphicsPixmapItem
{
    W_OBJECT(QGraphicsSelectablePixmapToggle)
    Q_INTERFACES(QGraphicsItem)

    const QPixmap m_pressed, m_pressed_selected, m_released, m_released_selected;
    bool m_toggled{};
    bool m_selected{};

public:
    QGraphicsSelectablePixmapToggle(
            QPixmap pressed,
            QPixmap pressed_selected,
            QPixmap released,
            QPixmap released_selected,
            QGraphicsItem* parent);

    void toggle();
    void setSelected(bool selected);
    void setState(bool toggled);

    bool state() const noexcept { return m_toggled; }

public:
    void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};
}
