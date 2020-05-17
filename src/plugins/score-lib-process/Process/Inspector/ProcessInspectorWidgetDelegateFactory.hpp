#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/plugins/Interface.hpp>
#include <score/tools/SafeCast.hpp>
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
  ~InspectorWidgetDelegateFactory() override;
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
      const InspectedObjects& objects,
      const score::DocumentContext& doc,
      QWidget* parent) const final override;
  bool matches(const InspectedObjects& objects) const final override;

protected:
  static QWidget* wrap(
      const Process::ProcessModel& process,
      const score::DocumentContext& doc,
      QWidget* widg,
      QWidget* parent);
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
    auto w = new Widget_T{safe_cast<const Process_T&>(process), doc, nullptr};

    return wrap(process, doc, w, parent);
  }

  bool matches_process(const Process::ProcessModel& process) const override
  {
    return dynamic_cast<const Process_T*>(&process);
  }
};

class DefaultInspectorWidgetDelegateFactory
    : public Process::InspectorWidgetDelegateFactory
{
  SCORE_CONCRETE("07c0f07b-f996-4aa9-88b9-664486ddbb00")
private:
  QWidget* make_process(
      const Process::ProcessModel& process,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return wrap(process, doc, nullptr, parent);
  }

  bool matches_process(const Process::ProcessModel& process) const override
  {
    return true;
  }
};
}
