#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class ScenarioModel;
class ScenarioInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ScenarioInspectorWidget(
                const ScenarioModel& object,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString); // TODO make a ProcessInspectorWidget

    private:
        const ScenarioModel& m_model;
};
