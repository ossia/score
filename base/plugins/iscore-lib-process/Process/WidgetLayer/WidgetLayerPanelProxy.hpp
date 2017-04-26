#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <iscore_lib_process_export.h>

namespace Process
{
class ProcessModel;
}
class QObject;

namespace WidgetLayer
{
template <typename Process_T, typename Widget_T>
class LayerPanelProxy final : public Process::LayerPanelProxy
{
public:
  explicit LayerPanelProxy(const Process::ProcessModel& vm, QObject* parent)
      : Process::LayerPanelProxy{parent}, m_layer{vm}
  {
    m_widget = new Widget_T{safe_cast<const Process_T&>(vm),
                            iscore::IDocument::documentContext(vm), nullptr};
  }

  const Process::ProcessModel& layer() final override
  {
    return m_layer;
  }

  QWidget* widget() const final override
  {
    return m_widget;
  }

private:
  const Process::ProcessModel& m_layer;
  QWidget* m_widget{};
};
}
