#pragma once
#include <Curve/CurveStyle.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <State/MessageListSerialization.hpp>
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationView.hpp>

namespace Interpolation
{
class Presenter final : public Curve::CurveProcessPresenter<ProcessModel, View>
{
    Q_OBJECT
public:
  Presenter(
      const Curve::Style& style,
      const ProcessModel& layer,
      View* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    con(m_layer, &ProcessModel::tweenChanged, this,
        &Presenter::on_tweenChanges);
    connect(
        m_view, &View::dropReceived, this,
        &Presenter::on_dropReceived);

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

  void on_dropReceived(const QPointF&, const QMimeData& mime)
  {
    auto& autom = this->model();
    // TODO refactor with AddressEditWidget && AutomationPresenter
    if (mime.formats().contains(score::mime::addressettings()))
    {
      Mime<Device::FullAddressSettings>::Deserializer des{mime};
      Device::FullAddressSettings as = des.deserialize();

      if (as.address.path.isEmpty())
        return;

      CommandDispatcher<> disp{context().context.commandStack};
      ChangeInterpolationAddress(model(), State::AddressAccessor{std::move(as.address)}, disp);
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
      ChangeInterpolationAddress(model(), std::move(newAddr), disp);
    }
  }

};
}
