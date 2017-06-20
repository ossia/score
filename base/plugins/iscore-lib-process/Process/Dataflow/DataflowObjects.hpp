#pragma once
#include <iscore/model/path/Path.hpp>
#include <State/Address.hpp>
#include <ossia/detail/optional.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <ossia/dataflow/graph_edge.hpp>
namespace QtNodes {
class Connection;
}
namespace Process
{
class DataflowProcess;
enum class CableType { ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict };

struct CableData
{
  CableType type;
  Path<DataflowProcess> source, sink;
  ossia::optional<int> outlet, inlet;

  friend bool operator==(const CableData& lhs, const CableData& rhs);
};

struct Cable
    : public IdentifiedObject<Cable>
{
  Cable() = delete;
  Cable(const Cable&) = delete;
  Cable(Id<Cable> c): IdentifiedObject{c, "Cable", nullptr} { }

  Cable(const iscore::DocumentContext& ctx, Id<Cable> c, const CableData& data):
    IdentifiedObject{c, "Cable", nullptr}
  {
    type = data.type;
    source = &data.source.find(ctx);
    sink = &data.sink.find(ctx);
    outlet = data.outlet;
    inlet = data.inlet;
  }

  QtNodes::Connection* gui{};
  CableType type{};
  DataflowProcess* source{};
  DataflowProcess* sink{};
  ossia::optional<int> outlet, inlet;
  std::shared_ptr<ossia::graph_node> source_node;
  std::shared_ptr<ossia::graph_node> sink_node;
  std::shared_ptr<ossia::graph_edge> exec;
};

enum class PortType { Message, Audio, Midi };
struct Port
{
  PortType type;
  QString customData;
  State::AddressAccessor address;

  friend bool operator==(const Port& lhs, const Port& rhs)
  {
    return lhs.type == rhs.type && lhs.customData == rhs.customData && lhs.address == rhs.address;
  }
};

}
