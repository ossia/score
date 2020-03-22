#pragma once
#include <Automation/Commands/ChangeAddress.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessContext.hpp>
#include <State/UpdateAddress.hpp>

#include <Metronome/MetronomeModel.hpp>
#include <Metronome/MetronomeView.hpp>
#include <verdigris>

namespace Metronome
{
class LayerPresenter final
    : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
  W_OBJECT(LayerPresenter)
public:
  LayerPresenter(
      const Curve::Style& style,
      const Metronome::ProcessModel& layer,
      LayerView* view,
      const Process::Context& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    connect(
        m_view,
        &LayerView::dropReceived,
        this,
        &LayerPresenter::on_dropReceived);
  }

private:
  void setFullView() override { m_curve.setBoundedMove(false); }

  void on_dropReceived(const QPointF& pos, const QMimeData& mime)
  {
    if(auto addr = State::onUpdatableAddress(model().address(), mime))
    {
      CommandDispatcher<>{context().context.commandStack}
        .submit(new ChangeMetronomeAddress{model(), *addr});
    }
  }
};
}
