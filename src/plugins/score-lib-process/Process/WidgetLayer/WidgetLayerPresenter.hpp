#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>


#include <score_lib_process_export.h>

namespace WidgetLayer
{
template <typename Process_T, typename Widget_T>
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Process::ProcessModel& model,
      View* view,
      const Process::Context& ctx,
      QObject* parent)
      : LayerPresenter{model, view, ctx, parent}, m_layer{model}, m_view{view}
  {
    putToFront();
    connect(view, &View::pressed, this, [&]() {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(
        m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);

    m_view->setWidget(
        new Widget_T{static_cast<const Process_T&>(m_layer), ctx, nullptr});
  }

  void setWidth(qreal width, qreal defaultWidth) override { m_view->setWidth(width); }
  void setHeight(qreal val) override { m_view->setHeight(val); }

  void putToFront() override { m_view->setVisible(true); }

  void putBehind() override { m_view->setVisible(false); }

  void on_zoomRatioChanged(ZoomRatio) override {}

  void parentGeometryChanged() override {}

  const Process::ProcessModel& model() const override { return m_layer; }
  const Id<Process::ProcessModel>& modelId() const override
  {
    return m_layer.id();
  }

private:
  const Process::ProcessModel& m_layer;
  View* m_view{};
};
}
