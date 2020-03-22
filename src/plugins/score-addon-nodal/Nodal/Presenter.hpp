#pragma once
#include <Nodal/Process.hpp>
#include <Process/Dataflow/NodeItem.hpp>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/model/Identifier.hpp>
namespace Nodal
{
class Model;
class View;
class Presenter final
    : public Process::LayerPresenter
    , public Nano::Observer
{
public:
  explicit Presenter(
      const Model& model, View* view,
      const Process::Context& ctx, QObject* parent);
  ~Presenter();

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  void on_created(Process::ProcessModel& n);
  void on_removing(const Process::ProcessModel& n);

private:
  IdContainer<Process::NodeItem, Process::ProcessModel> m_nodes;

  const Model& m_model;
  qreal m_defaultW{};
  ZoomRatio m_ratio{1.};
  View* m_view{};
  QMetaObject::Connection m_con;
};
}
