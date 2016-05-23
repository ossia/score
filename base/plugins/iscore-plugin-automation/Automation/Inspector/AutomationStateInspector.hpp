#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class QLabel;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Automation
{
class ProcessState;
class StateInspectorWidget final : public Inspector::InspectorWidgetBase
{
    public:
        explicit StateInspectorWidget(
                const ProcessState& object,
                const iscore::DocumentContext& context,
                QWidget* parent = nullptr);

    private:
        void on_stateChanged();

        const ProcessState& m_state;
        QLabel* m_label{};
};
}
