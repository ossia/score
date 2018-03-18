#pragma once
#include <Process/TimeValue.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/widgets/SpinBoxes.hpp>

class QComboBox;
class QSpinBox;
class QCheckBox;
namespace Scenario
{
namespace Settings
{

class View : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  View();

  void setSkin(const QString&);
  void setZoom(const int); // zoom percentage
  void setSlotHeight(const qreal);
  void setDefaultDuration(const TimeVal& t);
  void setAutoSequence(bool);
  void setDefaultEditor(QString);
  SETTINGS_UI_TOGGLE_HPP(TimeBar)

Q_SIGNALS:
  void SkinChanged(const QString&);
  void DefaultEditorChanged(QString);

  void zoomChanged(int);
  void SlotHeightChanged(qreal);
  void DefaultDurationChanged(const TimeVal& t);

  void AutoSequenceChanged(bool);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QComboBox* m_skin{};
  QLineEdit* m_editor{};
  QSpinBox* m_zoomSpinBox{};
  QSpinBox* m_slotHeightBox{};
  score::TimeSpinBox* m_defaultDur{};

  QCheckBox* m_sequence{};
};
}
}
