#include "Inspector.hpp"
#include <iscore/document/DocumentContext.hpp>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

namespace Skeleton
{
InspectorWidget::InspectorWidget(
        const Skeleton::Model& object,
        const iscore::DocumentContext& context,
        QWidget* parent):
    InspectorWidgetDelegate_T {object, parent},
    m_dispatcher{context.commandStack}
{
    auto lay = new QFormLayout{this};
    lay->addWidget(new QLabel("change me"));
}

InspectorWidget::~InspectorWidget()
{

}
}
