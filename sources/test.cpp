#include <QtGui>

#include "timeboxmodel.hpp"
#include "timeboxsmallpresenter.hpp"
#include "timeboxsmallview.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QGraphicsScene scene(0, 0, 500, 500);

    TimeboxModel *pModel = new TimeboxModel(100, 100, 300, 300);
    TimeboxSmallView *pItem = new TimeboxSmallView(pModel);
    TimeboxSmallPresenter *pPrez = new TimeboxSmallPresenter(pModel, pItem);
    scene.addItem(pItem);

    QGraphicsView view(&scene);
    view.showMaximized();

    return app.exec();
}
