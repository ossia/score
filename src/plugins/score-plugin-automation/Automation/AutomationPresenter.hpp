#pragma once
#include <State/UpdateAddress.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/ProcessContext.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Magnetism/MagnetismAdjuster.hpp>

#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationView.hpp>
#include <Automation/Commands/ChangeAddress.hpp>

#include <verdigris>

namespace Automation
{
class LayerPresenter final : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
  W_OBJECT(LayerPresenter)
public:
  LayerPresenter(
      const Curve::Style& style, const Automation::ProcessModel& layer, LayerView* view,
      const Process::Context& context, QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    // TODO instead have a prettyNameChanged signal.
    con(layer, &ProcessModel::tweenChanged, this, &LayerPresenter::on_tweenChanges);

    connect(m_view, &LayerView::dropReceived, this, &LayerPresenter::on_dropReceived);

    on_tweenChanges(layer.tween());
    con(layer.curve(), &Curve::Model::curveReset, this,
        [&] { on_tweenChanges(layer.tween()); });

    // Points snap to what the timeline snaps to: the grid, the other
    // processes' points. Asked for the curve rather than the automation, so
    // that the automation's own points, the dragged one among them, do not
    // attract it.
    auto& magnetism = (Process::MagnetismAdjuster&)
                          context.app.interfaces<Process::MagnetismAdjuster>();
    m_curve.setMagnetism([&layer, &magnetism](double x) {
      const TimeVal dur = layer.duration();
      if(dur.impl <= 0)
        return x;
      const auto info = magnetism.getPosition(
          &layer.curve(), TimeVal{int64_t(x * double(dur.impl))});
      return double(info.time.impl) / double(dur.impl);
    });
  }

private:
  void setFullView() override { m_curve.setBoundedMove(false); }

  void on_tweenChanges(bool b)
  {
    for(Curve::SegmentView& seg : m_curve.segments())
    {
      if(seg.model().start().x() != 0.)
      {
        seg.setTween(false);
      }
      else
      {
        seg.setTween(b);
      }
    }
  }

  void on_dropReceived(const QPointF& pos, const QMimeData& mime)
  {
    if(auto addr = State::onUpdatableAddress(model().address(), mime))
    {
      CommandDispatcher<>{context().context.commandStack}.submit(
          new ChangeAddress{model(), *addr});
    }
    else if(mime.hasFormat(score::mime::processpreset()))
    {
      handlePresetDrop(pos, mime);
    }
  }
};
}
