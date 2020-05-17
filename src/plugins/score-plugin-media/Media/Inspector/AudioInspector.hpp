#pragma once

#include <Media/Sound/SoundModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/SpinBoxes.hpp>

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

namespace Media
{
namespace Sound
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Sound::ProcessModel>
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
  QComboBox m_mode;

  score::SpinBox<double> m_tempo;
};
}
}
