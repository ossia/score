#pragma once
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationView.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <State/MessageListSerialization.hpp>

#include <verdigris>

namespace Automation
{
class LayerPresenter final
    : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
  W_OBJECT(LayerPresenter)
public:
  LayerPresenter(
      const Curve::Style& style,
      const Automation::ProcessModel& layer,
      LayerView* view,
      const Process::Context& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    // TODO instead have a prettyNameChanged signal.
    con(m_layer,
        &ProcessModel::tweenChanged,
        this,
        &LayerPresenter::on_tweenChanges);

    connect(
        m_view,
        &LayerView::dropReceived,
        this,
        &LayerPresenter::on_dropReceived);

    on_tweenChanges(m_layer.tween());
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

  void on_dropReceived(const QPointF& pos, const QMimeData& mime)
  {
    auto& autom = this->model();
    // TODO refactor with AddressEditWidget
    if (mime.formats().contains(score::mime::messagelist()))
    {
      Mime<State::MessageList>::Deserializer des{mime};
      State::MessageList ml = des.deserialize();
      if (ml.empty())
        return;
      auto& newAddr = ml[0].address;

      if (newAddr == autom.address())
        return;

      if (newAddr.address.path.isEmpty())
        return;

      CommandDispatcher<> disp{context().context.commandStack};
      disp.submit(new ChangeAddress{autom, newAddr});
    }
    else if (mime.formats().contains(score::mime::layerdata()))
    {
    }
  }
};
}
