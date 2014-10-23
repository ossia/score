#include "mainbox.h"
#include "ui_mainbox.h"

#include "utils.hpp"
#include "graphicsview.hpp"

MainBox::MainBox(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainBox)
{
    ui->setupUi(this);

    view = ui->graphicsView;
//    main = new Timebox(0, view, QPointF(0,0), 700, 500, FULL);
    timeBar = new TimeBarWidget(view);
}

MainBox::~MainBox()
{
    delete ui;
}
