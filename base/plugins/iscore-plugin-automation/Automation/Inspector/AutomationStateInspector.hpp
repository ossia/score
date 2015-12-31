#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class QLabel;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Automation
{
class State;
class StateInspectorWidget final : public InspectorWidgetBase
{
    public:
        explicit StateInspectorWidget(
                const State& object,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

    private:
        void on_stateChanged();

        const State& m_state;
        QLabel* m_label{};
};
}
