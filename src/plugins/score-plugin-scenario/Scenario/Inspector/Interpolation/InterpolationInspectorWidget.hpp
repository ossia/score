#pragma once
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Interpolation/InterpolationProcess.hpp>
class QLabel;
class QCheckBox;
namespace Interpolation
{
class ProcessModel;
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Interpolation::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

private:
  void on_addressChange(const ::State::AddressAccessor& newText);
  Device::AddressAccessorEditWidget* m_lineEdit{};
  QCheckBox* m_tween{};
  QLabel* m_label;

  CommandDispatcher<> m_dispatcher;
  void on_tweenChanged();
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("5159eabc-cd5c-4a00-a790-bd58936aace0")
};

class StateInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  explicit StateInspectorWidget(
      const ProcessState& object,
      const score::DocumentContext& context,
      QWidget* parent = nullptr);

private:
  void on_stateChanged();

  const ProcessState& m_state;
  QLabel* m_label{};
};

class StateInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("ea035d49-1897-4413-94e4-e5d6c90b21e6")
public:
  StateInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
