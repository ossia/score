#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/plugins/Interface.hpp>
#include <score/tools/SafeCast.hpp>

#include <score_lib_process_export.h>
class QBoxLayout;
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
  virtual QWidget* makeProcess(
      const Process::ProcessModel&, const score::DocumentContext& doc,
      QWidget* parent) const
      = 0;
  virtual bool matchesProcess(const Process::ProcessModel&) const = 0;

  virtual void addButtons(
      const Process::ProcessModel&, const score::DocumentContext& doc,
      QBoxLayout* layout, QWidget* parent) const;

  bool matchesProcess(
      const Process::ProcessModel& proc, const score::DocumentContext& doc,
      QWidget* parent) const;

  QWidget* make(
      const InspectedObjects& objects, const score::DocumentContext& doc,
      QWidget* parent) const final override;
  bool matches(const InspectedObjects& objects) const final override;

protected:
  QWidget* wrap(
      Process::ProcessModel& process, const score::DocumentContext& doc, QWidget* widg,
      QWidget* parent) const;
};

template <typename Process_T, typename Widget_T>
class InspectorWidgetDelegateFactory_T : public Process::InspectorWidgetDelegateFactory
{
private:
  QWidget* makeProcess(
      const Process::ProcessModel& process, const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    auto w = new Widget_T{safe_cast<const Process_T&>(process), doc, nullptr};

    return wrap(const_cast<Process::ProcessModel&>(process), doc, w, parent);
  }

  bool matchesProcess(const Process::ProcessModel& process) const override
  {
    return dynamic_cast<const Process_T*>(&process);
  }
};

}
