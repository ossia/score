#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>

#include <ossia/network/value/value_conversion.hpp>
#include <Media/Step/Commands.hpp>
#include <Media/Step/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>
namespace Media
{
namespace Step
{
class Model;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Process::ProcessModel& model, View* view, const Process::Context& ctx,
      QObject* parent)
      : LayerPresenter{model, view, ctx, parent}
      , m_view{view}
      , m_disp{m_context.context.commandStack}
  {
    putToFront();
    auto& m = static_cast<const Step::Model&>(model);

    connect(view, &View::stepsEdited, this, [&](const ossia::float_vector& v) {
      m_disp.submit(m, ossia::float_vector{v});
    });

    connect(view, &View::released, this, [&] { m_disp.commit(); });
    // On the timeline the bars take every click, so nothing else would focus
    // the layer, and the layer menu only opens on the focused one. A node opens
    // the menu itself: focusing there too would open a second one over it.
    if(!qobject_cast<Process::NodeItem*>(parent))
      connect(view, &View::pressed, this, [this] {
        m_context.context.focusDispatcher.focus(this);
      });

    con(m, &Step::Model::stepsChanged, this, [&] { m_view->update(); });
    con(m, &Step::Model::stepCountChanged, this, [&] { m_view->update(); });
    con(m, &Step::Model::currentSequenceChanged, this, [&] { m_view->update(); });
    con(*m.stepDuration, &Process::ControlInlet::valueChanged, this,
        [&] { on_zoomRatioChanged(m_ratio); });
  }

  void setWidth(qreal width, qreal defaultWidth) override { m_view->setWidth(width); }
  void setHeight(qreal val) override { m_view->setHeight(val); }

  void putToFront() override { m_view->setVisible(true); }

  void putBehind() override { m_view->setVisible(false); }

  void on_zoomRatioChanged(ZoomRatio r) override
  {
    m_ratio = r;
    auto& m = static_cast<const Step::Model&>(m_process);
    // {seconds, 0}, or {fraction of a whole note, sync}: drawn at 120 BPM
    // then, the tempo the layer does not know.
    const auto d = ossia::convert<ossia::vec2f>(m.stepDuration->value());
    const double seconds = d[1] != 0.f ? d[0] * 4. * 60. / 120. : d[0];
    m_view->setBarWidth(TimeVal::fromMsecs(seconds * 1000.).toPixels(r));
  }

  void parentGeometryChanged() override { }

  void fillContextMenu(
      QMenu& menu, QPoint pos, QPointF scenepos,
      const Process::LayerContextMenuManager& mgr) override
  {
    LayerPresenter::fillContextMenu(menu, pos, scenepos, mgr);
    fillStepsMenu(
        menu, static_cast<const Step::Model&>(m_process), m_context.context.commandStack);
  }

private:
  View* m_view{};
  SingleOngoingCommandDispatcher<Media::ChangeSteps> m_disp;
  ZoomRatio m_ratio{};
};
}
}
