#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class AutomationState;
class QLabel;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

class AutomationStateInspector final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationStateInspector(
                const AutomationState& object,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

        void on_stateChanged();

    private:
        const AutomationState& m_state;
        QLabel* m_label{};
};
