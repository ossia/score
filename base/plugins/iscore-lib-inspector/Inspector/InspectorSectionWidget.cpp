#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <qnamespace.h>

#include <QPushButton>
#include <QToolButton>

#include "InspectorSectionWidget.hpp"

namespace Inspector
{
InspectorSectionWidget::InspectorSectionWidget(bool editable, QWidget* parent) :
    QWidget(parent)
{
    // HEADER : arrow button and name
    auto title = new QWidget{this};
    auto titleLayout = new iscore::MarginLess<QHBoxLayout>{title};

    m_unfoldBtn = new QToolButton{title};
    m_unfoldBtn->setAutoRaise(true);

    m_buttonTitle = new QPushButton{title};
    m_buttonTitle->setFlat(true);
    m_buttonTitle->setText("section name");
    m_buttonTitle->setStyleSheet("text-align: left;");

    m_sectionTitle = new QLineEdit{tr("Section Name")};
    connect(m_sectionTitle, &QLineEdit::editingFinished,
            this, [=] ()
    {
        emit nameChanged(m_sectionTitle->text());
    });
    if(editable)
        m_buttonTitle->hide();
    else
        m_sectionTitle->hide();

    m_deleteBtn = new QToolButton{title};
    m_deleteBtn->setHidden(true);

    titleLayout->addWidget(m_unfoldBtn);
    titleLayout->addWidget(m_sectionTitle);
    titleLayout->addWidget(m_buttonTitle);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_deleteBtn);

    // CONTENT
    m_container = new QWidget;
    m_container->setContentsMargins(0,0,0,0);
    m_containerLayout = new iscore::MarginLess<QVBoxLayout>{m_container};
    m_containerLayout->addStretch();

    // GENERAL
    auto globalLayout = new iscore::MarginLess<QVBoxLayout>{this};
    globalLayout->addWidget(title);
    globalLayout->addWidget(m_container);
    this->setContentsMargins(0,0,0,0);

    connect(m_unfoldBtn, &QAbstractButton::released,
            this, &InspectorSectionWidget::expand);
    connect(m_buttonTitle, &QAbstractButton::clicked,
            this, &InspectorSectionWidget::expand);
    connect(m_deleteBtn, &QToolButton::released,
            this, &InspectorSectionWidget::deletePressed);

    // INIT
    m_isUnfolded = true;
    m_unfoldBtn->setArrowType(Qt::DownArrow);
    m_deleteBtn->setText("X");
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
        m_unfoldBtn->setArrowType(Qt::DownArrow);
    }
    else
    {
        m_unfoldBtn->setArrowType(Qt::RightArrow);
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

void InspectorSectionWidget::showDeleteButton(bool b)
{
    m_deleteBtn->setHidden(!b);
}

QWidget* InspectorSectionWidget::titleWidget()
{
    return m_buttonTitle;
}
}
