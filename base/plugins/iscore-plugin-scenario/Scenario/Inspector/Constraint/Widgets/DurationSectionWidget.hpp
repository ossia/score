#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <Scenario/Palette/Tool.hpp>

class QCheckBox;
class QGridLayout;
class QLabel;
namespace iscore {
class TimeSpinBox;
}  // namespace iscore

namespace Scenario
{

class EditionGrid;
class PlayGrid;
class EditionSettings;
class ConstraintInspectorDelegate;
class ConstraintInspectorWidget;
class ConstraintModel;
class DurationWidget final : public QWidget
{
    public:
        DurationWidget(
                const Scenario::EditionSettings& set,
                const ConstraintInspectorDelegate&,
                ConstraintInspectorWidget* parent);

    private:
        EditionGrid* m_editingWidget{};
        PlayGrid* m_playingWidget{};
};
}
