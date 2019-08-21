#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/model/Identifier.hpp>

namespace Process
{
class Port;
class ControlInlet;
class PortFactoryList;
}
namespace score
{
struct DocumentContext;
}

namespace ControlSurface
{
class Model;
class View;
class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Model& model, View* view,
      const Process::ProcessPresenterContext& ctx, QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  const Process::ProcessModel& model() const override;
  const Id<Process::ProcessModel>& modelId() const override;

private:
  void setupInlet(
      Process::ControlInlet& inlet,
      const Process::PortFactoryList& portFactory,
      const score::DocumentContext& doc);
  void on_controlAdded(const Id<Process::Port>& id);
  void on_controlRemoved(const Process::Port& p);

  const Model& m_model;
  View* m_view{};
  struct Port;
  std::vector<Port> m_ports;
};
}
