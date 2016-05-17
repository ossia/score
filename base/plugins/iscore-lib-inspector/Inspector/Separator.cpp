#include <QBoxLayout>
#include <QFrame>
#include <QLayout>

#include "Separator.hpp"

namespace Inspector
{
HSeparator::HSeparator(QWidget *parent) :
    QWidget {parent}
{
    this->setLayout(new QVBoxLayout);
    QFrame* f = new QFrame;
    this->layout()->addWidget(f);

    f->setFrameShape(QFrame::HLine);
    f->setLineWidth(0);
    f->setMidLineWidth(0);

    f->setObjectName("SeparatorFrame");
}

HSeparator::~HSeparator()
{

}

VSeparator::VSeparator(QWidget *parent) :
    QWidget {parent}
{
    this->setLayout(new QHBoxLayout);
    QFrame* f = new QFrame;
    this->layout()->addWidget(f);

    f->setFrameShape(QFrame::VLine);
    f->setLineWidth(1);

    f->setObjectName("SeparatorFrame");
}

VSeparator::~VSeparator()
{

}
}
