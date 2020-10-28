#pragma once
#include <Automation/AutomationModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Color/GradientModel.hpp>
#include <Metronome/MetronomeModel.hpp>

class QLabel;
class QWidget;
class QCheckBox;

namespace score
{
struct DocumentContext;
}
namespace State
{
struct Address;
class UnitWidget;
}
namespace Device
{
struct FullAddressAccessorSettings;
class AddressAccessorEditWidget;
}
namespace Explorer
{
class DeviceExplorerModel;
}
class QDoubleSpinBox;

namespace Automation
{
class ProcessModel;
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Automation::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

private:
  void on_addressChange(const Device::FullAddressAccessorSettings& newText);
  void on_minValueChanged();
  void on_maxValueChanged();
  void on_tweenChanged();

  Device::AddressAccessorEditWidget* m_lineEdit{};
  QCheckBox* m_tween{};
  QDoubleSpinBox *m_minsb{}, *m_maxsb{};

  CommandDispatcher<> m_dispatcher;
};
}

namespace Gradient
{
class ProcessModel;
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Gradient::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

private:
  void on_tweenChanged();

  QCheckBox* m_tween{};

  CommandDispatcher<> m_dispatcher;
};
}

namespace Metronome
{
class ProcessModel;
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Metronome::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

private:
  void on_addressChange(const Device::FullAddressAccessorSettings& newText);

  Device::AddressAccessorEditWidget* m_lineEdit{};

  CommandDispatcher<> m_dispatcher;
};
}
