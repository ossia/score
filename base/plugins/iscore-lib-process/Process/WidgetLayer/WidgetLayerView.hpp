#pragma once
#include <Process/LayerView.hpp>
#include <QString>
#include <QGraphicsSimpleTextItem>
#include <iscore_lib_process_export.h>


class QGraphicsItem;
class QPainter;
class QQuickView;
class QGraphicsProxyWidget;
class QQuickItem;

namespace WidgetLayer
{
class WidgetTextItem;
class ISCORE_LIB_PROCESS_EXPORT View final : public Process::LayerView
{
        Q_OBJECT
    public:
        explicit View(QGraphicsItem* parent);

        void setWidget(QWidget*);

    signals:
        void pressed();

    private:
        void updateText();
        void paint_impl(QPainter*) const override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;

        QGraphicsProxyWidget* m_widg{};
};
}
