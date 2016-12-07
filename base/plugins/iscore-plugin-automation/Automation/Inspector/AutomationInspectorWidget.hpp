#pragma once
#include <Automation/AutomationModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QWidget;
class QCheckBox;

namespace iscore
{
class Document;
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
}
namespace Explorer
{
class AddressAccessorEditWidget;
class DeviceExplorerModel;
}
class QDoubleSpinBox;

namespace Automation
{
class ProcessModel;
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Automation::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const iscore::DocumentContext& context,
      QWidget* parent);

private:
  void on_addressChange(const Device::FullAddressAccessorSettings& newText);
  void on_minValueChanged();
  void on_maxValueChanged();
  void on_tweenChanged();
  void on_unitChanged();

  Explorer::AddressAccessorEditWidget* m_lineEdit{};
  QCheckBox* m_tween{};
  QDoubleSpinBox *m_minsb{}, *m_maxsb{};
  State::UnitWidget* m_uw{};

  CommandDispatcher<> m_dispatcher;
};
}
