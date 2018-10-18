#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/Interface.hpp>
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
class SCORE_LIB_PROCESS_EXPORT PortFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(Process::Port, "4d461658-5c27-4a12-ba97-3d9392561ece")
public:
  ~PortFactory() override;

  virtual Process::Port* load(const VisitorVariant&, QObject* parent) = 0;
  virtual Dataflow::PortItem* makeItem(
      Process::Inlet& port, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context);
  virtual Dataflow::PortItem* makeItem(
      Process::Outlet& port, const score::DocumentContext& ctx,
      QGraphicsItem* parent, QObject* context);

  virtual void setupInspector(
      Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
      Inspector::Layout& lay, QObject* context);
  virtual void setupInspector(
      Process::Outlet& port, const score::DocumentContext& ctx,
      QWidget* parent, Inspector::Layout& lay, QObject* context);
};

class SCORE_LIB_PROCESS_EXPORT PortFactoryList final
    : public score::InterfaceList<PortFactory>
{
public:
  using object_type = Process::Port;
  ~PortFactoryList();
  Process::Port* loadMissing(const VisitorVariant& vis, QObject* parent) const;
};

inline auto writeInlets(
    DataStreamWriter& wr, const Process::PortFactoryList& pl,
    Process::Inlets& ports, QObject* parent)
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
inline auto writeOutlets(
    DataStreamWriter& wr, const Process::PortFactoryList& pl,
    Process::Outlets& ports, QObject* parent)
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
inline auto writeInlets(
    const QJsonArray& wr, const Process::PortFactoryList& pl,
    Process::Inlets& ports, QObject* parent)
{
  for (const auto& json_vref : wr)
  {
    auto proc = deserialize_interface(
        pl, JSONObject::Deserializer{json_vref.toObject()}, parent);
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
inline auto writeOutlets(
    const QJsonArray& wr, const Process::PortFactoryList& pl,
    Process::Outlets& ports, QObject* parent)
{
  for (const auto& json_vref : wr)
  {
    auto proc = deserialize_interface(
        pl, JSONObject::Deserializer{json_vref.toObject()}, parent);
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
inline auto readPorts(
    DataStreamReader& wr, const Process::Inlets& ins,
    const Process::Outlets& outs)
{
  wr.m_stream << (int32_t)ins.size();
  for (auto v : ins)
    wr.readFrom(*v);
  wr.m_stream << (int32_t)outs.size();
  for (auto v : outs)
    wr.readFrom(*v);
}
inline auto writePorts(
    DataStreamWriter& wr, const Process::PortFactoryList& pl,
    Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  writeInlets(wr, pl, ins, parent);
  writeOutlets(wr, pl, outs, parent);
}
inline auto readPorts(
    QJsonObject& obj, const Process::Inlets& ins, const Process::Outlets& outs)
{
  obj["Inlets"] = toJsonArray(ins);
  obj["Outlets"] = toJsonArray(outs);
}
inline auto writePorts(
    const QJsonObject& obj, const Process::PortFactoryList& pl,
    Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  writeInlets(obj["Inlets"].toArray(), pl, ins, parent);
  writeOutlets(obj["Outlets"].toArray(), pl, outs, parent);
}
}
