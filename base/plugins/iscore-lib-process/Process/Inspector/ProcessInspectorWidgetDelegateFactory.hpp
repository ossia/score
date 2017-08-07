#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_lib_process_export.h>
namespace Process
{
class ProcessModel;
class StateProcess;
class InspectorWidgetDelegate;
class StateProcessInspectorWidgetDelegate;
}
namespace iscore
{
struct DocumentContext;
}

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT InspectorWidgetDelegateFactory
    : public iscore::Interface<InspectorWidgetDelegateFactory>
{
  ISCORE_INTERFACE("75a45c5e-24ab-4ebb-ba57-195254a6847f")
public:
  virtual ~InspectorWidgetDelegateFactory();
  virtual QWidget* make(
      const Process::ProcessModel&,
      const iscore::DocumentContext& doc,
      QWidget* parent) const = 0;
  virtual bool matches(const Process::ProcessModel&) const = 0;

  bool matches(
      const Process::ProcessModel& proc,
      const iscore::DocumentContext& doc,
      QWidget* parent) const
  {
    return matches(proc);
  }
};

template <typename Process_T, typename Widget_T>
class InspectorWidgetDelegateFactory_T
    : public Process::InspectorWidgetDelegateFactory
{
private:
  QWidget* make(
      const Process::ProcessModel& process,
      const iscore::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new Widget_T{safe_cast<const Process_T&>(process), doc, parent};
  }

  bool matches(const Process::ProcessModel& process) const override
  {
    return dynamic_cast<const Process_T*>(&process);
  }
};

class ISCORE_LIB_PROCESS_EXPORT StateProcessInspectorWidgetDelegateFactory
    : public iscore::
          Interface<StateProcessInspectorWidgetDelegateFactory>
{
  ISCORE_INTERFACE("707f7d38-7897-4b8f-81eb-737976b05ea6")
public:
  virtual ~StateProcessInspectorWidgetDelegateFactory();
  virtual QWidget* make(
      const Process::StateProcess&,
      const iscore::DocumentContext& doc,
      QWidget* parent) const = 0;
  virtual bool matches(const Process::StateProcess&) const = 0;

  bool matches(
      const Process::StateProcess& proc,
      const iscore::DocumentContext& doc,
      QWidget* parent) const
  {
    return matches(proc);
  }
};

template <typename Process_T, typename Widget_T>
class StateProcessInspectorWidgetDelegateFactory_T
    : public Process::StateProcessInspectorWidgetDelegateFactory
{
private:
  QWidget* make(
      const Process::StateProcess& process,
      const iscore::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new Widget_T{safe_cast<const Process_T&>(process), doc, parent};
  }

  bool matches(const Process::StateProcess& process) const override
  {
    return dynamic_cast<const Process_T*>(&process);
  }
};
}
