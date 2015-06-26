#pragma once
#include <QGraphicsObject>
#include <QBrush>
#include <ProcessInterface/LayerModel.hpp>
class ProcessPanelPresenter;
class ProcessPanelGraphicsProxy : public QGraphicsObject
{
        QSizeF m_size;
        QBrush m_bgBrush;

    public:
        ProcessPanelGraphicsProxy(const LayerModel& pvm, const ProcessPanelPresenter& pres);

        QRectF boundingRect() const;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

    public slots:
        void setSize(const QSizeF& size);
};
