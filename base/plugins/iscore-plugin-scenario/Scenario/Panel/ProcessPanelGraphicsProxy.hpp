#pragma once
#include <qgraphicsitem.h>
#include <qrect.h>
#include <qsize.h>

class LayerModel;
class ProcessPanelPresenter;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ProcessPanelGraphicsProxy final : public QGraphicsObject
{
        QSizeF m_size;

    public:
        ProcessPanelGraphicsProxy(const LayerModel& lm, const ProcessPanelPresenter& pres);

        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    public slots:
        void setSize(const QSizeF& size);
};
