#pragma once
#include <QWidget>

#include <score_lib_process_export.h>
namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT InspectorWidgetDelegate : public QWidget
{
public:
  using QWidget::QWidget;
  virtual ~InspectorWidgetDelegate();
};

template <typename Process_T>
class InspectorWidgetDelegate_T : public InspectorWidgetDelegate
{
public:
  InspectorWidgetDelegate_T(const Process_T& process, QWidget* parent)
      : InspectorWidgetDelegate{parent}, m_process{process}
  {
  }

  ~InspectorWidgetDelegate_T() = default;

  const Process_T& process() const noexcept { return m_process; }

private:
  const Process_T& m_process;
};

class ProcessModel;
}
