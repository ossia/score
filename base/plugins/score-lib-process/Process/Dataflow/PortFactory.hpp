#pragma once
#include <Process/Dataflow/Port.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT PortFactory
    : public score::Interface<Process::Port>
{
  SCORE_INTERFACE("4d461658-5c27-4a12-ba97-3d9392561ece")
public:
  ~PortFactory() override;

  virtual Process::Port* load(const VisitorVariant&, QObject* parent) = 0;
};

class SCORE_LIB_PROCESS_EXPORT PortFactoryList final
    : public score::InterfaceList<PortFactory>
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
  { return Metadata<ConcreteKey_k, Model_T>::get(); }

  Model_T* load(
      const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

using InletFactory = PortFactory_T<Inlet>;
using ControlInletFactory = PortFactory_T<ControlInlet>;
using OutletFactory = PortFactory_T<Outlet>;
using ControlOutletFactory = PortFactory_T<ControlOutlet>;


inline
auto writeInlets(DataStreamWriter& wr, const Process::PortFactoryList& pl, Process::Inlets& ports, QObject* parent)
{
  int32_t count;
  wr.m_stream >> count;

  for (; count-- > 0;)
  {
    auto proc = deserialize_interface(pl, wr, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Inlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
inline
auto writeOutlets(DataStreamWriter& wr, const Process::PortFactoryList& pl, Process::Outlets& ports, QObject* parent)
{
  int32_t count;
  wr.m_stream >> count;

  for (; count-- > 0;)
  {
    auto proc = deserialize_interface(pl, wr, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Outlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
inline
auto writeInlets(const QJsonArray& wr, const Process::PortFactoryList& pl, Process::Inlets& ports, QObject* parent)
{
  for(const auto& json_vref: wr)
  {
    auto proc = deserialize_interface(pl, JSONObject::Deserializer{json_vref.toObject()}, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Inlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
inline
auto writeOutlets(const QJsonArray& wr, const Process::PortFactoryList& pl, Process::Outlets& ports, QObject* parent)
{
  for(const auto& json_vref: wr)
  {
    auto proc = deserialize_interface(pl, JSONObject::Deserializer{json_vref.toObject()}, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Outlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
inline
auto readPorts(DataStreamReader& wr, const Process::Inlets& ins, const Process::Outlets& outs)
{
  wr.m_stream << (int32_t)ins.size();
  for(auto v : ins)
    wr.readFrom(*v);
  wr.m_stream << (int32_t)outs.size();
  for(auto v : outs)
    wr.readFrom(*v);
}
inline
auto writePorts(DataStreamWriter& wr, const Process::PortFactoryList& pl, Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  writeInlets(wr, pl, ins, parent);
  writeOutlets(wr, pl, outs, parent);
}
inline
auto readPorts(QJsonObject& obj, const Process::Inlets& ins, const Process::Outlets& outs)
{
  obj["Inlets"] = toJsonArray(ins);
  obj["Outlets"] = toJsonArray(outs);
}
inline
auto writePorts(const QJsonObject& obj, const Process::PortFactoryList& pl, Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  writeInlets(obj["Inlets"].toArray(), pl, ins, parent);
  writeOutlets(obj["Outlets"].toArray(), pl, outs, parent);
}

}
