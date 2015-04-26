#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetBase.hpp"


#include <QLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QScrollArea>

InspectorWidgetBase::InspectorWidgetBase(QObject* inspectedObj, QWidget* parent) :
    QWidget(parent),
    _inspectedObject {inspectedObj},
    m_commandDispatcher{inspectedObj
                          ? new CommandDispatcher<>{iscore::IDocument::documentFromObject(inspectedObj)->commandStack(),
                                                  this}
                          : nullptr},
    m_selectionDispatcher{inspectedObj
                           ? new iscore::SelectionDispatcher{iscore::IDocument::documentFromObject(inspectedObj)->selectionStack()}
                           : nullptr}

{
    _layout = new QVBoxLayout;
    _layout->setContentsMargins(0,1,0,0);
    setLayout(_layout);


    // scroll Area
    _scrollAreaLayout = new QVBoxLayout;
    QScrollArea* scrollArea = new QScrollArea;
    QWidget* scrollAreaContentWidget = new QWidget;
    scrollArea->setWidgetResizable(true);

    scrollAreaContentWidget->setLayout(_scrollAreaLayout);
    scrollArea->setWidget(scrollAreaContentWidget);

    _sections.push_back(scrollArea);

    updateSectionsView(_layout, _sections);

    _scrollAreaLayout->addStretch();


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

void InspectorWidgetBase::addHeader(QWidget* header)
{
    _sections.push_front(header);
    _layout->insertWidget(0, header);
}

void InspectorWidgetBase::setInspectedObject(QObject* object)
{
    _inspectedObject = object;
}

QObject* InspectorWidgetBase::inspectedObject() const
{
    return _inspectedObject;
}

