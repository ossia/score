#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

namespace Scenario { class ScenarioModel; }
class ScenarioInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ScenarioInspectorWidget(
                const Scenario::ScenarioModel& object,
                iscore::Document& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString); // TODO make a ProcessInspectorWidget

    private:
        const Scenario::ScenarioModel& m_model;
};
