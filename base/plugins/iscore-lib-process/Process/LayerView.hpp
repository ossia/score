#pragma once

#include <QtGlobal>
#include <QGraphicsItem>
#include <QRect>
#include <iscore_lib_process_export.h>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ISCORE_LIB_PROCESS_EXPORT LayerView : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;
        virtual ~LayerView();

        QRectF boundingRect() const final override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) final override;

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

    protected:
        virtual void paint_impl(QPainter*) const = 0;

    private:
        qreal m_height {};
        qreal m_width {};
};
