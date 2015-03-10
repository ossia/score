#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetBase.hpp"

#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QScrollArea>
#include <QScrollBar>
#include <QDebug>
#include <QColorDialog>
#include <QVector>

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
    _layout->setMargin(5);
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


void InspectorWidgetBase::addNewSection(QString sectionName,
                                        QWidget* content)
{
    InspectorSectionWidget* section = new InspectorSectionWidget(sectionName, this);
    section->addContent(content);
    _scrollAreaLayout->addWidget(section);
}

void InspectorWidgetBase::addSubSection(QString parentSection,
                                        QString subSection,
                                        InspectorSectionWidget* content)
{
    //TODO what about passing a pointer?
    InspectorSectionWidget* section = findChild<InspectorSectionWidget*>(parentSection);

    if(section != nullptr)
    {
        {
            content->renameSection(subSection);
            content->setObjectName(subSection);
            section->addContent(content);
        }
    }
}

void InspectorWidgetBase::insertSection(int index,
                                        QString name,
                                        QWidget* content)
{
    if(index < 0)
    {
        index += _scrollAreaLayout->count();
    }

    InspectorSectionWidget* section = new InspectorSectionWidget(this);
    section->renameSection(name);
    section->setObjectName(name);

    if(content)
    {
        section->addContent(content);
    }

    _scrollAreaLayout->insertWidget(index, section);
}

void InspectorWidgetBase::removeSection(QString sectionName)
{

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

