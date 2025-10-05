#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/model/Identifier.hpp>

namespace Process
{
class ControlInlet;
class PortFactoryList;
}
namespace Gfx::Video
{
class Model;
class View;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Model& model, View* view, const Process::Context& ctx, QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_drop(const QPointF& pos, const QMimeData& md);
  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  static constexpr double recommendedHeight = 55.;

private:
  View* m_view{};
};
}
