#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/serialization/VisitorCommon.hpp>
class QGraphicsItem;
namespace Inspector
{
class Layout;
}
namespace Dataflow
{
class PortItem;
}
namespace Process
{
struct Context;
class SCORE_LIB_PROCESS_EXPORT PortFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(Process::Port, "4d461658-5c27-4a12-ba97-3d9392561ece")
public:
  ~PortFactory() override;

  virtual Process::Port* load(const VisitorVariant&, QObject* parent) = 0;
  virtual Dataflow::PortItem* makeItem(
      Process::Inlet& port,
      const Process::Context& ctx,
      QGraphicsItem* parent,
      QObject* context);
  virtual Dataflow::PortItem* makeItem(
      Process::Outlet& port,
      const Process::Context& ctx,
      QGraphicsItem* parent,
      QObject* context);

  virtual void setupInletInspector(
      const Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context);
  virtual void setupOutletInspector(
      const Process::Outlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context);

  virtual QGraphicsItem* makeControlItem(
      Process::ControlInlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context);
  virtual QGraphicsItem* makeControlItem(
      Process::ControlOutlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context);

  virtual QWidget* makeControlWidget(
      Process::ControlInlet& port,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context);
};

class SCORE_LIB_PROCESS_EXPORT PortFactoryList final : public score::InterfaceList<PortFactory>
{
public:
  using object_type = Process::Port;
  ~PortFactoryList();
  Process::Port* loadMissing(const VisitorVariant& vis, QObject* parent) const;
};

template <typename Model_T>
class PortFactory_T final : public Process::PortFactory
{
public:
  ~PortFactory_T() override = default;

private:
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

SCORE_LIB_PROCESS_EXPORT
void readPorts(DataStreamReader& wr, const Process::Inlets& ins, const Process::Outlets& outs);

SCORE_LIB_PROCESS_EXPORT
void readPorts(JSONReader& obj, const Process::Inlets& ins, const Process::Outlets& outs);

SCORE_LIB_PROCESS_EXPORT
void writePorts(
    DataStreamWriter& wr,
    const Process::PortFactoryList& pl,
    Process::Inlets& ins,
    Process::Outlets& outs,
    QObject* parent);

SCORE_LIB_PROCESS_EXPORT
void writePorts(
    const JSONWriter& obj,
    const Process::PortFactoryList& pl,
    Process::Inlets& ins,
    Process::Outlets& outs,
    QObject* parent);
}
