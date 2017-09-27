#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Input/InputModel.hpp>
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
    CommandDispatcher<> m_dispatcher;
    QLineEdit m_edit;
    QSpinBox m_start;
    QSpinBox m_upmix;

};
}

namespace Input
{
class InspectorWidget final :
    public Process::InspectorWidgetDelegate_T<Input::ProcessModel>
{
  public:
    explicit InspectorWidget(
        const Input::ProcessModel& object,
        const score::DocumentContext& doc,
        QWidget* parent);

  private:
    CommandDispatcher<> m_dispatcher;

    QSpinBox m_start;
    QSpinBox m_count;

};
}
}
