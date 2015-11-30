#include <core/document/Document.hpp>
#include <QBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>

#include "InspectorWidgetBase.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectAbstract.hpp>

InspectorWidgetBase::InspectorWidgetBase(
        const IdentifiedObjectAbstract& inspectedObj,
        iscore::Document& doc,
        QWidget* parent) :
    QWidget(parent),
    m_inspectedObject {inspectedObj},
    m_document{doc},
    m_commandDispatcher(new CommandDispatcher<>{m_document.commandStack()}),
    m_selectionDispatcher(new iscore::SelectionDispatcher{m_document.selectionStack()})

{
    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0,1,0,0);
    setLayout(m_layout);
    m_layout->setSpacing(0);

    // scroll Area
    m_scrollAreaLayout = new QVBoxLayout;
    m_scrollAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollAreaLayout->setSpacing(0);
    QScrollArea* scrollArea = new QScrollArea;
    QWidget* scrollAreaContentWidget = new QWidget;
    scrollArea->setWidgetResizable(true);

    scrollAreaContentWidget->setLayout(m_scrollAreaLayout);
    scrollArea->setWidget(scrollAreaContentWidget);

    m_sections.push_back(scrollArea);

    updateSectionsView(m_layout, m_sections);
}

InspectorWidgetBase::~InspectorWidgetBase()
{
    delete m_commandDispatcher;
}

QString InspectorWidgetBase::tabName()
{
    return m_inspectedObject.objectName();
}

void InspectorWidgetBase::updateSectionsView(QVBoxLayout* layout,
                                             const std::list<QWidget*>& contents)
{
    while(! layout->isEmpty())
    {
        auto item = layout->takeAt(0);

        delete item->widget();
        delete item;
    }

    for(auto& section : contents)
    {
        layout->addWidget(section);
    }
}

void InspectorWidgetBase::updateAreaLayout(std::list<QWidget*>& contents)
{
    while(! m_scrollAreaLayout->isEmpty())
    {
        auto item = m_scrollAreaLayout->takeAt(m_scrollAreaLayout->count()-1);

        delete item->widget();
        delete item;
    }

    for(auto& section : contents)
    {
        m_scrollAreaLayout->addWidget(section);
    }
    m_scrollAreaLayout->addStretch(1);
}

void InspectorWidgetBase::addHeader(QWidget* header)
{
    m_sections.push_front(header);
    m_layout->insertWidget(0, header);
}

const IdentifiedObjectAbstract& InspectorWidgetBase::inspectedObject() const
{
    return m_inspectedObject;
}
