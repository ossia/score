#include "InspectorSectionWidget.hpp"

#include <QLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <iscore/widgets/MarginLess.hpp>
InspectorSectionWidget::InspectorSectionWidget(QWidget* parent) :
    QWidget(parent)
{
    // HEADER : arrow button and name
    auto title = new QWidget;
    auto titleLayout = new MarginLess<QHBoxLayout>;

    m_btn = new QToolButton;
    m_btn->setAutoRaise(true);

    m_buttonTitle = new QPushButton;
    m_buttonTitle->setFlat(true);
    m_buttonTitle->setText("section name");
    m_buttonTitle->setStyleSheet("text-align: left;");
    auto buttontitle_lay = new MarginLess<QVBoxLayout>;
    m_buttonTitle->setLayout(buttontitle_lay);

    titleLayout->addWidget(m_btn);
    titleLayout->addWidget(m_buttonTitle);
    title->setLayout(titleLayout);

    // CONTENT
    m_container = new QWidget;
    m_container->setContentsMargins(0,0,0,0);
    m_containerLayout = new MarginLess<QVBoxLayout>;
    m_containerLayout->addStretch();
    m_container->setLayout(m_containerLayout);

    // GENERAL
    auto globalLayout = new MarginLess<QVBoxLayout>;
    globalLayout->addWidget(title);
    globalLayout->addWidget(m_container);
    this->setContentsMargins(0,0,0,0);

    connect(m_btn, SIGNAL(released()), this, SLOT(expand()));
    connect(m_buttonTitle, SIGNAL(clicked()), this, SLOT(expand()));

    // INIT
    m_isUnfolded = true;
    m_btn->setArrowType(Qt::DownArrow);
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
    m_isUnfolded = !m_isUnfolded;
    m_container->setVisible(m_isUnfolded);

    if(m_isUnfolded)
    {
        m_btn->setArrowType(Qt::DownArrow);
    }
    else
    {
        m_btn->setArrowType(Qt::RightArrow);
    }
}

void InspectorSectionWidget::renameSection(QString newName)
{
//	_sectionTitle->setText (newName);
    m_buttonTitle->setText(newName);
}

void InspectorSectionWidget::addContent(QWidget* newWidget)
{
    m_containerLayout->addWidget(newWidget);
}

void InspectorSectionWidget::removeContent(QWidget* toRemove)
{
    m_containerLayout->removeWidget(toRemove);
    delete toRemove;
}

void InspectorSectionWidget::removeAll()
{
    while(QLayoutItem* item = m_containerLayout->takeAt(0))
    {
        if(QWidget* wid = item->widget())
        {
            delete wid;
        }

        delete item;
    }
}
