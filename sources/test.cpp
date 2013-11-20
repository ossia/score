#include <QtGui>

#include "timeboxmodel.hpp"
#include "timeboxpresenter.hpp"
#include "timeboxsmallview.hpp"
#include "timeevent.hpp"
#include <QPointF>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QGraphicsScene scene(0, 0, 600, 500);

    TimeEvent *event = new TimeEvent(QPointF(20,30), 0);
    scene.addItem(event);

    TimeboxModel *pModel = new TimeboxModel(100, 100, 400, 300);
    TimeboxSmallView *pItem = new TimeboxSmallView(pModel);
    TimeboxPresenter *pPrez = new TimeboxPresenter(pModel, pItem);
    scene.addItem(pItem);

    QGraphicsView view(&scene);
    view.showNormal();

    pPrez->setView(&view);

    return app.exec();
}
