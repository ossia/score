#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class QWidget;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
namespace iscore {
class Document;
}  // namespace iscore

class ScenarioInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ScenarioInspectorWidget(
                const Scenario::ScenarioModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString); // TODO make a ProcessInspectorWidget

    private:
        const Scenario::ScenarioModel& m_model;
};
