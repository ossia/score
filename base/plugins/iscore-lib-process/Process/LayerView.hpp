#pragma once

#include <QtGlobal>
#include <QGraphicsItem>
#include <QRect>
#include <iscore_lib_process_export.h>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT LayerView :
        public QObject,
        public QGraphicsItem
{
        Q_OBJECT
    public:
        LayerView(QGraphicsItem* parent):
            QGraphicsItem{parent}
        {

        }

        virtual ~LayerView();

        QRectF boundingRect() const final override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) final override;

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

    signals:
        void heightChanged();
        void widthChanged();

    protected:
        virtual void paint_impl(QPainter*) const = 0;

    private:
        qreal m_height {};
        qreal m_width {};
};
}
