#include <QBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <iscore/document/DocumentContext.hpp>

#include "InspectorWidgetBase.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectAbstract.hpp>
#include <iscore/widgets/MarginLess.hpp>

namespace Inspector
{
InspectorWidgetBase::InspectorWidgetBase(
        const IdentifiedObjectAbstract& inspectedObj,
        const iscore::DocumentContext& ctx,
        QWidget* parent) :
    QWidget(parent),
    m_inspectedObject {inspectedObj},
    m_context{ctx},
    m_commandDispatcher(new CommandDispatcher<>{ctx.commandStack}),
    m_selectionDispatcher(new iscore::SelectionDispatcher{ctx.selectionStack})

{
    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0,1,0,0);
    setLayout(m_layout);
    m_layout->setSpacing(0);

    // scroll Area
    auto scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
    auto scrollAreaContentWidget = new QWidget;
    m_scrollAreaLayout = new iscore::MarginLess<QVBoxLayout>{scrollAreaContentWidget};
    m_scrollAreaLayout->setSizeConstraint(QLayout::SetMinimumSize);
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
}
