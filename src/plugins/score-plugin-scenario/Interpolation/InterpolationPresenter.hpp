#pragma once
#include <Curve/CurveStyle.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <State/UpdateAddress.hpp>

#include <Interpolation/Commands/ChangeAddress.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationView.hpp>
#include <verdigris>

namespace Interpolation
{
class Presenter final : public Curve::CurveProcessPresenter<ProcessModel, View>
{
  W_OBJECT(Presenter)
public:
  Presenter(
      const Curve::Style& style,
      const ProcessModel& layer,
      View* view,
      const Process::Context& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    con(layer,
        &ProcessModel::tweenChanged,
        this,
        &Presenter::on_tweenChanges);
    connect(m_view, &View::dropReceived, this, &Presenter::on_dropReceived);

    on_tweenChanges(layer.tween());
    con(layer.curve(), &Curve::Model::curveReset, this, [&] {
      on_tweenChanges(layer.tween());
    });
  }

private:
  void setFullView() override { m_curve.setBoundedMove(false); }

  void on_tweenChanges(bool b)
  {
    for (Curve::SegmentView& seg : m_curve.segments())
    {
      if (seg.model().start().x() != 0.)
      {
        seg.setTween(false);
      }
      else
      {
        seg.setTween(b);
      }
    }
  }

  void on_dropReceived(const QPointF&, const QMimeData& mime)
  {
    if(auto newAddr = State::onUpdatableAddress(model().address(), mime))
    {
      CommandDispatcher<> disp{context().context.commandStack};
      ChangeInterpolationAddress(model(), std::move(*newAddr), disp);
    }
  }
};
}
