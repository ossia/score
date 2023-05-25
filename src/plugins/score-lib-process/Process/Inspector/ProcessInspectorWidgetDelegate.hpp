#pragma once
#include <ossia/detail/config.hpp>

#include <QWidget>

#include <score_lib_process_export.h>
namespace Process
{
class ProcessModel;

template <typename Process_T>
class InspectorWidgetDelegate_T : public QWidget
{
public:
  OSSIA_INLINE
  InspectorWidgetDelegate_T(const Process_T& process, QWidget* parent) noexcept
      : QWidget{parent}
      , m_process{process}
  {
  }

  OSSIA_INLINE ~InspectorWidgetDelegate_T() noexcept = default;

  OSSIA_INLINE const Process_T& process() const noexcept { return m_process; }

private:
  const Process_T& m_process;
};

}
