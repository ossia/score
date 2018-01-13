#pragma once
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <score_lib_process_export.h>
namespace Process
{
class ProcessModel;
class InspectorWidgetDelegate;
}
namespace score
{
struct DocumentContext;
}

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT InspectorWidgetDelegateFactory
    : public Inspector::InspectorWidgetFactory
{
public:
  virtual ~InspectorWidgetDelegateFactory();
  virtual QWidget* make_process(
      const Process::ProcessModel&,
      const score::DocumentContext& doc,
      QWidget* parent) const = 0;
  virtual bool matches_process(const Process::ProcessModel&) const = 0;

  bool matches_process(
      const Process::ProcessModel& proc,
      const score::DocumentContext& doc,
      QWidget* parent) const
  {
    return matches_process(proc);
  }

  QWidget* make(
       const QList<const QObject*>& objects,
       const score::DocumentContext& doc,
       QWidget* parent) const final override;
   bool matches(const QList<const QObject*>& objects) const final override;
};

template <typename Process_T, typename Widget_T>
class InspectorWidgetDelegateFactory_T
    : public Process::InspectorWidgetDelegateFactory
{
private:
  QWidget* make_process(
      const Process::ProcessModel& process,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new Widget_T{safe_cast<const Process_T&>(process), doc, parent};
  }

  bool matches_process(const Process::ProcessModel& process) const override
  {
    return dynamic_cast<const Process_T*>(&process);
  }
};

}
