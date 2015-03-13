#include "ScenarioInspectorWidget.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>

#include "Process/ScenarioModel.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

ScenarioInspectorWidget::ScenarioInspectorWidget(ScenarioModel* object, QWidget* parent) :
    InspectorWidgetBase {nullptr},
    m_model {object}
{
    setObjectName("ScenarioInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;
    vec.push_back(new QLabel{ QString::number(object->id_val())} );

    QPushButton* displayBtn = new QPushButton {tr("Display in new Deck"), this};
    vec.push_back(displayBtn);

    connect(displayBtn, &QPushButton::clicked,
            [=] ()
    {
        emit createViewInNewDeck(QString::number(m_model->id_val()));
    });

    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}
