#pragma once
#include <Media/Metro/MetroCommands.hpp>
#include <Media/Metro/MetroView.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>

namespace Media
{
namespace Metro
{
class Model;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Process::ProcessModel& model,
      View* view,
      const Process::Context& ctx,
      QObject* parent)
      : LayerPresenter{model, view, ctx, parent}
      , m_view{view}
  {
    putToFront();

    connect(view, &View::pressed, this, [&] {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(
        m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);
  }

  void setWidth(qreal width, qreal defaultWidth) override { m_view->setWidth(width); }
  void setHeight(qreal val) override { m_view->setHeight(val); }

  void putToFront() override { m_view->setVisible(true); }

  void putBehind() override { m_view->setVisible(false); }

  void on_zoomRatioChanged(ZoomRatio r) override { }

  void parentGeometryChanged() override {}

private:
  View* m_view{};
};
}
}
