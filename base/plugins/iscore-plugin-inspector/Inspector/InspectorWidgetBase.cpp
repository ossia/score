#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetBase.hpp"

#include <QLineEdit>
#include <QTextEdit>
#include <QScrollArea>

InspectorWidgetBase::InspectorWidgetBase(
        const QObject* inspectedObj,
        QWidget* parent) :
    QWidget(parent),
    _inspectedObject {inspectedObj},
    m_commandDispatcher(inspectedObj
                          ? new CommandDispatcher<>{iscore::IDocument::documentFromObject(inspectedObj)->commandStack()}
                          : nullptr),
    m_selectionDispatcher(inspectedObj
                           ? new iscore::SelectionDispatcher{iscore::IDocument::documentFromObject(inspectedObj)->selectionStack()}
                           : nullptr)

{
    _layout = new QVBoxLayout;
    _layout->setContentsMargins(0,1,0,0);
    setLayout(_layout);
    _layout->setMargin(0);
    _layout->setSpacing(0);


    // scroll Area
    m_scrollAreaLayout = new QVBoxLayout;
    m_scrollAreaLayout->setMargin(0);
    m_scrollAreaLayout->setSpacing(0);
    QScrollArea* scrollArea = new QScrollArea;
    QWidget* scrollAreaContentWidget = new QWidget;
    scrollArea->setWidgetResizable(true);

    scrollAreaContentWidget->setLayout(m_scrollAreaLayout);
    scrollArea->setWidget(scrollAreaContentWidget);

    _sections.push_back(scrollArea);

    updateSectionsView(_layout, _sections);
}

InspectorWidgetBase::~InspectorWidgetBase()
{
    delete m_commandDispatcher;
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
        auto item = m_scrollAreaLayout->takeAt(0);

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
    _sections.push_front(header);
    _layout->insertWidget(0, header);
}

// TODO replace with const reference and prevent this.
void InspectorWidgetBase::setInspectedObject(const QObject* object)
{
    _inspectedObject = object;
}

const QObject* InspectorWidgetBase::inspectedObject() const
{
    return _inspectedObject;
}


