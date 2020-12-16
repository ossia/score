#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Palette/Tool.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QCheckBox;
class QGridLayout;
class QFormLayout;
class QLabel;

namespace Scenario
{
class EditionGrid;
class PlayGrid;
class EditionSettings;
class IntervalInspectorWidget;
class IntervalModel;
class DurationWidget final : public QWidget
{
public:
  DurationWidget(
      const Scenario::EditionSettings& set,
      QFormLayout& lay,
      IntervalInspectorWidget* parent);

private:
  EditionGrid* m_editingWidget{};
  // PlayGrid* m_playingWidget{};
};
}
