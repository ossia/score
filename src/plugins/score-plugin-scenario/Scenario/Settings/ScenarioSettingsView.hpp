#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/widgets/SpinBoxes.hpp>

#include <score_plugin_scenario_export.h>

#include <verdigris>

class QComboBox;
class QTabWidget;
class QSpinBox;
class QCheckBox;

namespace score
{
class FormWidget;
class TimeSpinBox;
}
namespace Scenario
{
namespace Settings
{

class SCORE_PLUGIN_SCENARIO_EXPORT View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setSlotHeight(const qreal);
  void setDefaultDuration(const TimeVal& t);
  void setAutoSequence(bool);
  void setDefaultEditor(QString);
  void setSkin(const QString&);
  void setZoom(const int); // zoom percentage
  SETTINGS_UI_COMBOBOX_HPP(ScriptEditorPlacement)
  SETTINGS_UI_COMBOBOX_HPP(ProcessUIPlacement)
  SETTINGS_UI_COMBOBOX_HPP(ScriptEditorPreview)
  SETTINGS_UI_SPINBOX_HPP(UpdateRate)
  SETTINGS_UI_SPINBOX_HPP(ExecutionRefreshRate)
  SETTINGS_UI_TOGGLE_HPP(ExecutionUpdate)
  SETTINGS_UI_TOGGLE_HPP(TimeBar)
  SETTINGS_UI_TOGGLE_HPP(MeasureBars)
  SETTINGS_UI_TOGGLE_HPP(MagneticMeasures)

public:
  void DefaultEditorChanged(QString arg_1) W_SIGNAL(DefaultEditorChanged, arg_1);
  void SkinChanged(const QString& arg_1) W_SIGNAL(SkinChanged, arg_1);
  void zoomChanged(int arg_1) W_SIGNAL(zoomChanged, arg_1);

  void SlotHeightChanged(int arg_1) W_SIGNAL(SlotHeightChanged, arg_1);
  void DefaultDurationChanged(const TimeVal& t) W_SIGNAL(DefaultDurationChanged, t);

  void AutoSequenceChanged(bool arg_1) W_SIGNAL(AutoSequenceChanged, arg_1);

private:
  QWidget* getWidget() override;

  //! The page is a tab widget so that the skin editor gets its own sub-tab,
  //! the way the Effects settings hold one tab per plug-in format.
  QTabWidget* m_tabs{};
  score::FormWidget* m_widg{};
  QSpinBox* m_zoomSpinBox{};
  class SkinEditorWidget* m_skinEditor{};

  //! The presenter pushes the current skin at construction, but the editor is
  //! only built when the settings page is first shown, so hold the value
  //! until there is something to put it in.
  QString m_pendingSkin;

  QLineEdit* m_editor{};
  QSpinBox* m_slotHeightBox{};
  score::TimeSpinBox* m_defaultDur{};

  QCheckBox* m_sequence{};
};
}
}
