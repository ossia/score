#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <QPainter>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>
/*
#include <QQuickWidget>
#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QGraphicsLayout>
*/
class QGraphicsItem;

DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{
    /*
    auto obj = new QGraphicsProxyWidget(this);
    obj->setAutoFillBackground(false);

    auto widg = new QQuickWidget;
    widg->setStyleSheet("background-color: rgba(0,0,0,0)");
    widg->setAutoFillBackground(false);
    widg->setSource(QUrl("qrc:/DummyProcess.qml"));
    widg->setClearColor(Qt::transparent);
    widg->setAttribute(Qt::WA_TranslucentBackground, true);
    widg->show();

    obj->setWidget(widg);
    */
}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    auto f = ProcessFonts::Sans();
    f.setPointSize(30);
    painter->setFont(f);
    painter->setPen(Qt::lightGray);

    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}
