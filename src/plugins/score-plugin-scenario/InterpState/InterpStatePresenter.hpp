#pragma once
#include <Curve/CurveStyle.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <InterpState/InterpStateProcess.hpp>
#include <InterpState/InterpStateView.hpp>

#include <verdigris>

namespace InterpState
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
  }
};
}
