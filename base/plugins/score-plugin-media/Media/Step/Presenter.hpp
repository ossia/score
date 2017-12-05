#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Media/Step/View.hpp>
#include <Media/Step/Commands.hpp>
#include <ossia/detail/math.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

namespace Media
{
namespace Step
{
class Model;
class Presenter final : public Process::LayerPresenter
{
  public:
    explicit Presenter(
        const Process::ProcessModel& model,
        View* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent)
      : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
    {
      putToFront();
      auto& m = static_cast<const Step::Model&>(model);
      connect(view, &View::pressed, this, [&] {
        m_context.context.focusDispatcher.focus(this);
      });
      connect(view, &View::change, this, [&] (int num, float v) {
        if(num < 0)
          return;

        auto vec = m.steps();
        if(num > vec.size())
        {
          vec.resize(num);
        }
        v = ossia::clamp(v, 0.f, 1.f);
        vec[num] = v;

        CommandDispatcher<>{ctx.commandStack}
            .submitCommand(new ChangeSteps(m, std::move(vec)));
      });

      connect(
            m_view, &View::askContextMenu, this,
            &Presenter::contextMenuRequested);

      con(m, &Step::Model::stepsChanged,
          this, [&] { m_view->update(); });

      view->m_model = &m;
    }

    void setWidth(qreal val) override
    {
      m_view->setWidth(val);
    }
    void setHeight(qreal val) override
    {
      m_view->setHeight(val);
    }

    void putToFront() override
    {
      m_view->setVisible(true);
    }

    void putBehind() override
    {
      m_view->setVisible(false);
    }

    void on_zoomRatioChanged(ZoomRatio) override
    {
    }

    void parentGeometryChanged() override
    {
    }

    const Process::ProcessModel& model() const override
    {
      return m_layer;
    }
    const Id<Process::ProcessModel>& modelId() const override
    {
      return m_layer.id();
    }

  private:
    const Process::ProcessModel& m_layer;
    View* m_view{};
};

}
}
