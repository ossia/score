#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Process/LayerPresenter.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <Spline/Commands.hpp>
#include <Spline/Model.hpp>

namespace Spline
{

class View;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Spline::ProcessModel& model,
      Spline::View* view,
      const Process::Context& ctx,
      QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

private:
  View* m_view{};
};
}
