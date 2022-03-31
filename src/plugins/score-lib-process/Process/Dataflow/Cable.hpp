#pragma once
#include <State/Address.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/Selectable.hpp>

#include <QUuid>

#include <score_lib_process_export.h>

#include <verdigris>

namespace ossia
{
class graph_node;
struct outlet;
struct inlet;
struct graph_edge;
using node_ptr = std::shared_ptr<graph_node>;
using outlet_ptr = outlet*;
using inlet_ptr = inlet*;
using edge_ptr = std::shared_ptr<graph_edge>;
}
namespace Dataflow
{
class DocumentPlugin;
}
namespace Process
{
class Node;
class Port;
class Outlet;
class Inlet;
class Cable;
enum class CableType
{
  ImmediateGlutton,
  ImmediateStrict,
  DelayedGlutton,
  DelayedStrict
};

struct SCORE_LIB_PROCESS_EXPORT CableData
{
  CableType type{};
  Path<Process::Port> source, sink;

  SCORE_LIB_PROCESS_EXPORT
  friend bool operator==(const CableData& lhs, const CableData& rhs);
};

class SCORE_LIB_PROCESS_EXPORT Cable final : public IdentifiedObject<Cable>
{
  W_OBJECT(Cable)
  SCORE_SERIALIZE_FRIENDS

public:
  Selectable selection;
  Cable() = delete;
  ~Cable();
  Cable(const Cable&) = delete;
  Cable(Id<Cable> c, QObject* parent);

  template <typename T>
  Cable(T&& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }
  Cable(Id<Cable> c, const CableData& data, QObject* parent);

  void update(const CableData&);

  CableData toCableData() const;

  CableType type() const;
  Path<Process::Outlet> source() const;
  Path<Process::Inlet> sink() const;

  void setType(CableType type);
  void resetCache() const noexcept override;

public:
  void typeChanged(CableType type)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, typeChanged, type)

private:
  CableType m_type{};
  Path<Process::Outlet> m_source;
  Path<Process::Inlet> m_sink;

  W_PROPERTY(Path<Process::Inlet>, sink READ sink)

  W_PROPERTY(Path<Process::Outlet>, source READ source)

  W_PROPERTY(CableType, type READ type WRITE setType NOTIFY typeChanged)
};
}

DEFAULT_MODEL_METADATA(Process::Cable, "Cable")

W_REGISTER_ARGTYPE(Id<Process::Cable>)
W_REGISTER_ARGTYPE(Process::CableType)
