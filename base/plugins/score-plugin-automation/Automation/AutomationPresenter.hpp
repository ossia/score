#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationView.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Automation/Commands/ChangeAddress.hpp>
#include <Process/ProcessContext.hpp>
#include <State/MessageListSerialization.hpp>

namespace Automation
{
class LayerPresenter final
    : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
    Q_OBJECT
public:
  LayerPresenter(
      const Curve::Style& style,
      const Automation::ProcessModel& layer,
      LayerView* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    // TODO instead have a prettyNameChanged signal.
    con(m_layer, &ProcessModel::tweenChanged, this,
        &LayerPresenter::on_tweenChanges);

    connect(
        m_view, &LayerView::dropReceived, this,
        &LayerPresenter::on_dropReceived);

    on_tweenChanges(m_layer.tween());
    con(layer.curve(), &Curve::Model::curveReset,
        this, [&] {
      on_tweenChanges(layer.tween());
    });
  }

private:
  void setFullView() override
  {
    m_curve.setBoundedMove(false);
  }

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
    if (mime.formats().contains(score::mime::addressettings()))
    {
      Mime<Device::FullAddressSettings>::Deserializer des{mime};
      Device::FullAddressSettings as = des.deserialize();

      if (as.address.path.isEmpty())
        return;

      CommandDispatcher<> disp{context().context.commandStack};
      disp.submitCommand(new ChangeAddress{autom, std::move(as)});
    }
    else if (mime.formats().contains(score::mime::messagelist()))
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
      disp.submitCommand(new ChangeAddress{autom, newAddr});
    }
  }
};
}
