#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/widgets/SpinBoxes.hpp>

#include <verdigris>

class QComboBox;
class QSpinBox;
class QCheckBox;

namespace score { class FormWidget;}
namespace Scenario
{
namespace Settings
{

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setSkin(const QString&);
  void setZoom(const int); // zoom percentage
  void setSlotHeight(const qreal);
  void setDefaultDuration(const TimeVal& t);
  void setAutoSequence(bool);
  void setDefaultEditor(QString);
  SETTINGS_UI_TOGGLE_HPP(TimeBar)
  SETTINGS_UI_TOGGLE_HPP(MeasureBars)
  SETTINGS_UI_TOGGLE_HPP(MagneticMeasures)

public:
  void SkinChanged(const QString& arg_1) W_SIGNAL(SkinChanged, arg_1);
  void DefaultEditorChanged(QString arg_1) W_SIGNAL(DefaultEditorChanged, arg_1);

  void zoomChanged(int arg_1) W_SIGNAL(zoomChanged, arg_1);
  void SlotHeightChanged(int arg_1) W_SIGNAL(SlotHeightChanged, arg_1);
  void DefaultDurationChanged(const TimeVal& t) W_SIGNAL(DefaultDurationChanged, t);

  void AutoSequenceChanged(bool arg_1) W_SIGNAL(AutoSequenceChanged, arg_1);

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};

  QComboBox* m_skin{};
  QLineEdit* m_editor{};
  QSpinBox* m_zoomSpinBox{};
  QSpinBox* m_slotHeightBox{};
  score::TimeSpinBox* m_defaultDur{};

  QCheckBox* m_sequence{};
};
}
}
