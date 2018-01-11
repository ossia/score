#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/Metronome/MetronomeModel.hpp>
#include <Automation/Metronome/MetronomeView.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Automation/Commands/ChangeAddress.hpp>
#include <Process/ProcessContext.hpp>
#include <State/MessageListSerialization.hpp>

namespace Metronome
{
class LayerPresenter final
    : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
    Q_OBJECT
public:
  LayerPresenter(
      const Curve::Style& style,
      const Metronome::ProcessModel& layer,
      LayerView* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    connect(
        m_view, &LayerView::dropReceived, this,
        &LayerPresenter::on_dropReceived);
  }

private:
  void setFullView() override
  {
    m_curve.setBoundedMove(false);
  }

  void on_dropReceived(const QPointF& pos,const QMimeData& mime)
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
      disp.submitCommand(new ChangeMetronomeAddress{autom, std::move(as.address)});
    }
    else if (mime.formats().contains(score::mime::messagelist()))
    {
      Mime<State::MessageList>::Deserializer des{mime};
      State::MessageList ml = des.deserialize();
      if (ml.empty())
        return;
      auto& newAddr = ml[0].address;

      if (newAddr.address == autom.address())
        return;

      if (newAddr.address.path.isEmpty())
        return;

      CommandDispatcher<> disp{context().context.commandStack};
      disp.submitCommand(new ChangeMetronomeAddress{autom, newAddr.address});
    }
  }
};
}
