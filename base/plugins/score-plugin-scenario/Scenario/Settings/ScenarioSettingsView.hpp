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

class View : public score::SettingsDelegateView
{
  Q_OBJECT
public:
  View();

  void setSkin(const QString&);
  void setZoom(const int); // zoom percentage
  void setSlotHeight(const qreal);
  void setDefaultDuration(const TimeVal& t);
  void setSnapshot(bool);
  void setSequence(bool);

signals:
  void skinChanged(const QString&);
  void zoomChanged(int);
  void slotHeightChanged(qreal);
  void defaultDurationChanged(const TimeVal& t);
  void snapshotChanged(bool);
  void sequenceChanged(bool);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QComboBox* m_skin{};
  QSpinBox* m_zoomSpinBox{};
  QSpinBox* m_slotHeightBox{};
  score::TimeSpinBox* m_defaultDur{};
  QCheckBox* m_snapshot{};
  QCheckBox* m_sequence{};
};
}
}
