#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <qnamespace.h>

#include <QPushButton>
#include <QMenu>
#include <QToolButton>
#include <QAction>

#include "InspectorSectionWidget.hpp"
namespace Inspector
{
InspectorSectionWidget::InspectorSectionWidget(
        bool editable,
        QWidget* parent) :
    QWidget(parent)
{
    // HEADER : arrow button and name
    auto title = new QWidget{this};
    auto titleLayout = new iscore::MarginLess<QHBoxLayout>{title};

    m_unfoldBtn = new QToolButton{title};
    m_unfoldBtn->setAutoRaise(true);

    m_buttonTitle = new QPushButton{title};
    m_buttonTitle->setObjectName("ButtonTitle");

    m_buttonTitle->setFlat(true);
    m_buttonTitle->setText("section name");
    m_buttonTitle->setStyleSheet("text-align: left;");

    m_sectionTitle = new QLineEdit{tr("Section Name")};
    m_sectionTitle->setObjectName("SectionTitle");
    connect(m_sectionTitle, &QLineEdit::editingFinished,
            this, [=] ()
    {
        emit nameChanged(m_sectionTitle->text());
    });
    if(editable)
        m_buttonTitle->hide();
    else
        m_sectionTitle->hide();

    m_menuBtn = new QPushButton{"#", title};
    m_menuBtn->setObjectName("SettingsMenu");
    m_menuBtn->setHidden(true);
    m_menu = new QMenu{m_menuBtn};
    m_menuBtn->setMenu(m_menu);


    titleLayout->addWidget(m_unfoldBtn);
    titleLayout->addWidget(m_sectionTitle);
    titleLayout->addWidget(m_buttonTitle);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_menuBtn);

    // CONTENT
    m_container = new QWidget;
    m_container->setObjectName("InspectorContainer");
    m_container->setContentsMargins(0,0,0,0);
    m_containerLayout = new iscore::MarginLess<QVBoxLayout>{m_container};

    // GENERAL
    auto globalLayout = new iscore::MarginLess<QVBoxLayout>{this};
    globalLayout->addWidget(title);
    globalLayout->addWidget(m_container);
    this->setContentsMargins(0,0,0,0);

    connect(m_unfoldBtn, &QAbstractButton::released,
            this, [&] { this->expand(!m_isUnfolded); } );
    connect(m_buttonTitle, &QAbstractButton::clicked,
            this, [&] { this->expand(!m_isUnfolded); } );

    // INIT
    m_isUnfolded = true;
    m_unfoldBtn->setArrowType(Qt::DownArrow);
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

QString InspectorSectionWidget::name() const
{
    return m_sectionTitle->text();
}

void InspectorSectionWidget::expand(bool b)
{
    if(m_isUnfolded == b)
        return;
    else
    m_isUnfolded = b;
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

void InspectorSectionWidget::showMenu(bool b)
{
    m_menuBtn->setHidden(!b);
}

QWidget* InspectorSectionWidget::titleWidget()
{
    return m_buttonTitle;
}
}
