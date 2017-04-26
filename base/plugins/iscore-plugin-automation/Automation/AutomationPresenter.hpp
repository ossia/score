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
public:
  LayerPresenter(
      const Curve::Style& style,
      const ProcessModel& layer,
      LayerView* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    // TODO instead have a prettyNameChanged signal.
    con(m_layer, &ProcessModel::addressChanged, this,
        &LayerPresenter::on_nameChanges);
    con(m_layer, &ProcessModel::tweenChanged, this,
        &LayerPresenter::on_tweenChanges);
    con(m_layer.metadata(), &iscore::ModelMetadata::NameChanged,
        this, &LayerPresenter::on_nameChanges);

    connect(
        m_view, &LayerView::dropReceived, this,
        &LayerPresenter::on_dropReceived);
    on_nameChanges();
    m_view->showName(true);

    on_tweenChanges(m_layer.tween());
  }

private:
  void on_tweenChanges(bool b)
  {
    for (auto& seg : m_curvepresenter->segments())
    {
      if (seg.model().start().x() == 0.)
      {
        seg.setTween(b);
        return;
      }
    }
  }

  void on_nameChanges()
  {
    m_view->setDisplayedName(m_layer.prettyName());
  }

  void on_dropReceived(const QMimeData& mime)
  {
    auto& autom = this->model();
    // TODO refactor with AddressEditWidget
    if (mime.formats().contains(iscore::mime::addressettings()))
    {
      Mime<Device::FullAddressSettings>::Deserializer des{mime};
      Device::FullAddressSettings as = des.deserialize();

      if (as.address.path.isEmpty())
        return;

      CommandDispatcher<> disp{context().context.commandStack};
      disp.submitCommand(new ChangeAddress{autom, std::move(as)});
    }
    else if (mime.formats().contains(iscore::mime::messagelist()))
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
