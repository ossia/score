#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class AutomationState;
class AutomationStateInspector : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationStateInspector(const AutomationState& object,
                                          QWidget* parent = 0);

        void on_stateChanged();

    private:
        const AutomationState& m_state;
        QLabel* m_label{};
};
