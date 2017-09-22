#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QCheckBox;
class QGridLayout;
class QLabel;
namespace score
{
class TimeSpinBox;
} // namespace score

namespace Scenario
{

class EditionGrid;
class PlayGrid;
class EditionSettings;
class IntervalInspectorDelegate;
class IntervalInspectorWidget;
class IntervalModel;
class DurationWidget final : public QWidget
{
public:
  DurationWidget(
      const Scenario::EditionSettings& set,
      const IntervalInspectorDelegate&,
      IntervalInspectorWidget* parent);

private:
  EditionGrid* m_editingWidget{};
  PlayGrid* m_playingWidget{};
};
}
