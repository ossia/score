#include "ScenarioInspectorWidget.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>

#include <QLabel>
#include <QVBoxLayout>

ScenarioInspectorWidget::ScenarioInspectorWidget(ScenarioModel* object, QWidget* parent) :
    InspectorWidgetBase {nullptr}
{
    setObjectName("ScenarioInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;
    vec.push_back(new QLabel{"TODO"});

    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}
