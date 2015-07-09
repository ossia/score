#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetBase.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QLineEdit>
#include <QTextEdit>
#include <QScrollArea>

// TODO pass the commandStack & selectionStack in the ctor instead.
// via the inspector control's currentDocument ?
InspectorWidgetBase::InspectorWidgetBase(
        const QObject* inspectedObj,
        QWidget* parent) :
    QWidget(parent),
    m_inspectedObject {inspectedObj},
    m_commandDispatcher(inspectedObj
                          ? new CommandDispatcher<>{iscore::IDocument::documentFromObject(inspectedObj)->commandStack()}
                          : nullptr),
    m_selectionDispatcher(inspectedObj
                           ? new iscore::SelectionDispatcher{iscore::IDocument::documentFromObject(inspectedObj)->selectionStack()}
                           : nullptr)

{
    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0,1,0,0);
    setLayout(m_layout);
    m_layout->setMargin(0);
    m_layout->setSpacing(0);

    // scroll Area
    m_scrollAreaLayout = new QVBoxLayout;
    m_scrollAreaLayout->setMargin(0);
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
    return m_inspectedObject->objectName();
}

void InspectorWidgetBase::updateSectionsView(QVBoxLayout* layout,
                                             QVector<QWidget*>& contents)
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

void InspectorWidgetBase::updateAreaLayout(QVector<QWidget*>& contents)
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

const QObject* InspectorWidgetBase::inspectedObject() const
{
    return m_inspectedObject;
}
