#include <iscore/widgets/MarginLess.hpp>
#include <qboxlayout.h>
#include <qlayoutitem.h>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>

#include "InspectorSectionWidget.hpp"

InspectorSectionWidget::InspectorSectionWidget(bool editable, QWidget* parent) :
    QWidget(parent)
{
    // HEADER : arrow button and name
    auto title = new QWidget;
    auto titleLayout = new iscore::MarginLess<QHBoxLayout>;

    m_btn = new QToolButton;
    m_btn->setAutoRaise(true);

    m_buttonTitle = new QPushButton;
    m_buttonTitle->setFlat(true);
    m_buttonTitle->setText("section name");
    m_buttonTitle->setStyleSheet("text-align: left;");
    auto buttontitle_lay = new iscore::MarginLess<QVBoxLayout>;
    m_buttonTitle->setLayout(buttontitle_lay);

    m_sectionTitle = new QLineEdit{tr("Section Name")};
    connect(m_sectionTitle, &QLineEdit::editingFinished,
            this, [=] ()
    {
        emit nameChanged(m_sectionTitle->text());
    });

    titleLayout->addWidget(m_btn);
    if(editable)
        titleLayout->addWidget(m_sectionTitle);
    else
        titleLayout->addWidget(m_buttonTitle);

    title->setLayout(titleLayout);

    // CONTENT
    m_container = new QWidget;
    m_container->setContentsMargins(0,0,0,0);
    m_containerLayout = new iscore::MarginLess<QVBoxLayout>;
    m_containerLayout->addStretch();
    m_container->setLayout(m_containerLayout);

    // GENERAL
    auto globalLayout = new iscore::MarginLess<QVBoxLayout>;
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

InspectorSectionWidget::InspectorSectionWidget(QString name, bool editable, QWidget* parent) :
    InspectorSectionWidget(editable, parent)
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
    m_sectionTitle->setText(newName);
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

QWidget* InspectorSectionWidget::titleWidget()
{
    return m_buttonTitle;
}
