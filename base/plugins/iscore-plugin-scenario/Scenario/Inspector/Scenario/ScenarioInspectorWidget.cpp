#include "ScenarioInspectorWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

ScenarioInspectorWidget::ScenarioInspectorWidget(
        const ScenarioModel& object,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {object, doc, parent},
    m_model {object}
{
    setObjectName("ScenarioInspectorWidget");
    setParent(parent);

    std::list<QWidget*> vec;

    QPushButton* displayBtn = new QPushButton {tr("Display in new Slot"), this};
    vec.push_back(displayBtn);

    connect(displayBtn, &QPushButton::clicked,
            [=] ()
    {
        emit createViewInNewSlot(QString::number(m_model.id_val()));
    });

    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}
