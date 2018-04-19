#include "Inspector.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <score/document/DocumentContext.hpp>

namespace Skeleton
{
InspectorWidget::InspectorWidget(
    const Skeleton::Model& object,
    const score::DocumentContext& context,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
    , m_dispatcher{context.commandStack}
{
  auto lay = new QFormLayout{this};
  lay->addWidget(new QLabel("change me"));
}

InspectorWidget::~InspectorWidget()
{
}
}
