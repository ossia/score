#pragma once
#include <QGraphicsObject>
#include <ProcessInterface/ProcessViewModel.hpp>
class ProcessPanelPresenter;
class ProcessPanelGraphicsProxy : public QGraphicsObject
{
        const ProcessViewModel& m_pvm;
        const ProcessPanelPresenter& m_pres;
        QSizeF m_size;

    public:
        ProcessPanelGraphicsProxy(const ProcessViewModel& pvm, const ProcessPanelPresenter& pres);

        QRectF boundingRect() const;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

    public slots:
        void setSize(const QSizeF& size);
};
