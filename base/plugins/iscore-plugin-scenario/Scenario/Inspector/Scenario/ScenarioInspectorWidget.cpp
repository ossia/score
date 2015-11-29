#include <Scenario/Process/ScenarioModel.hpp>
#include <QBoxLayout>

#include <QPushButton>
#include <list>

#include <Inspector/InspectorWidgetBase.hpp>
#include "ScenarioInspectorWidget.hpp"

class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

ScenarioInspectorWidget::ScenarioInspectorWidget(
        const Scenario::ScenarioModel& object,
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
