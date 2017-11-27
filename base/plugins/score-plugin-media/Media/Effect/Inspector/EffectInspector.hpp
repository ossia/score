#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Media/Effect/EffectProcessModel.hpp>

class QListWidget;
class QPushButton;

namespace Media
{
namespace Effect
{
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const score::DocumentContext& doc,
                QWidget* parent);

    private:
        void recreate();
        QListWidget* m_list{};
        QPushButton* m_add{};
        CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        SCORE_CONCRETE("cc8ceff3-ef93-4b73-865a-a9f870d6e898")
};
}
}

