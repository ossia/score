#include "InspectorSectionWidget.hpp"

#include <QLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>

InspectorSectionWidget::InspectorSectionWidget(QWidget* parent) :
    QWidget(parent)
{
    // HEADER : arrow button and name
    QWidget* title = new QWidget;
    QHBoxLayout* titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0,0,0,0);

    _btn = new QToolButton;
    _btn->setAutoRaise(true);

    _buttonTitle = new QPushButton;
    _buttonTitle->setFlat(true);
    _buttonTitle->setText("section name");
    _buttonTitle->setStyleSheet("text-align: left;");
    auto buttontitle_lay = new QVBoxLayout;
    buttontitle_lay->setSpacing(0);
    _buttonTitle->setLayout(buttontitle_lay);
//	_buttonTitle->layout()->addWidget (_sectionTitle);
//    _buttonTitle->layout()->setMargin(0);
//	_sectionTitle->hide();

    titleLayout->addWidget(_btn);
    titleLayout->addWidget(_buttonTitle);
    title->setLayout(titleLayout);
    titleLayout->setSpacing(0);

    // CONTENT
    _container = new QWidget;
    _container->setContentsMargins(0,0,0,0);
    _containerLayout = new QVBoxLayout;
    _containerLayout->setContentsMargins(5,0,0,0);
    _containerLayout->addStretch();
    _containerLayout->setSpacing(0);
    _container->setLayout(_containerLayout);

    // GENERAL
    QVBoxLayout* globalLayout = new QVBoxLayout;
    globalLayout->addWidget(title);
    globalLayout->addWidget(_container);
    globalLayout->setContentsMargins(0,0,0,0);
    globalLayout->setSpacing(0);
    this->setContentsMargins(0,0,0,0);

    connect(_btn, SIGNAL(released()), this, SLOT(expand()));
    connect(_buttonTitle, SIGNAL(clicked()), this, SLOT(expand()));
//	connect (_sectionTitle, SIGNAL (editingFinished() ), this, SLOT (nameEditDisable() ) );


    // INIT
    _isUnfolded = true;
    _btn->setArrowType(Qt::DownArrow);
    //   expend();

    setLayout(globalLayout);

    renameSection("Section Name");
}

InspectorSectionWidget::InspectorSectionWidget(QString name, QWidget* parent) :
    InspectorSectionWidget(parent)
{
    renameSection(name);
    setObjectName(name);
}

InspectorSectionWidget::~InspectorSectionWidget()
{

}

void InspectorSectionWidget::expand()
{
    _isUnfolded = !_isUnfolded;
    _container->setVisible(_isUnfolded);

    if(_isUnfolded)
    {
        _btn->setArrowType(Qt::DownArrow);
    }
    else
    {
        _btn->setArrowType(Qt::RightArrow);
    }
}

void InspectorSectionWidget::renameSection(QString newName)
{
//	_sectionTitle->setText (newName);
    _buttonTitle->setText(newName);
}

void InspectorSectionWidget::addContent(QWidget* newWidget)
{
    _containerLayout->addWidget(newWidget);
}

void InspectorSectionWidget::removeContent(QWidget* toRemove)
{
    _containerLayout->removeWidget(toRemove);
    delete toRemove;
}

void InspectorSectionWidget::removeAll()
{
    while(QLayoutItem* item = _containerLayout->takeAt(0))
    {
        if(QWidget* wid = item->widget())
        {
            delete wid;
        }

        delete item;
    }
}
