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
namespace Gfx::Mesh
{
class Model;
class View;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(const Model& model, View* view, const Process::Context& ctx, QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

private:
  const Model& m_model;
  View* m_view{};
  void setupInlet(
      Process::ControlInlet& port,
      const Process::PortFactoryList& portFactory,
      const Process::Context& doc);
  struct Port;
  std::vector<Port> m_ports;
};
}
