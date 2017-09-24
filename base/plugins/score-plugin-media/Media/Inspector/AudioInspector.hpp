#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <QLineEdit>
#include <QSpinBox>

namespace Media
{
namespace Sound
{
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Sound::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const score::DocumentContext& doc,
                QWidget* parent);

    private:
        QLineEdit m_edit;
        QSpinBox m_start;
        QSpinBox m_upmix;

        CommandDispatcher<> m_dispatcher;
};
}
}
