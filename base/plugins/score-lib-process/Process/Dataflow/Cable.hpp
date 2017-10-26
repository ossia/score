#pragma once
#include <State/Address.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score_lib_process_export.h>
#include <QPointer>

namespace ossia
{
class graph_node;
struct outlet;
struct inlet;
struct graph_edge;
using node_ptr = std::shared_ptr<graph_node>;
using outlet_ptr = std::shared_ptr<outlet>;
using inlet_ptr = std::shared_ptr<inlet>;
using edge_ptr = std::shared_ptr<graph_edge>;
}
namespace Dataflow
{
class NodeItem;
class DocumentPlugin;
}
namespace Process
{
class Node;
class Port;
class Cable;
enum class CableType { ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict };

struct SCORE_LIB_PROCESS_EXPORT CableData
{
    CableType type{};
    Path<Process::Port> source, sink;

    SCORE_LIB_PROCESS_EXPORT
    friend bool operator==(const CableData& lhs, const CableData& rhs);
};

class SCORE_LIB_PROCESS_EXPORT Cable
    : public IdentifiedObject<Cable>
{
    Q_OBJECT
    Q_PROPERTY(CableType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(Process::Port* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Process::Port* sink READ sink WRITE setSink NOTIFY sinkChanged)
  public:
    Cable() = delete;
    ~Cable();
    Cable(const Cable&) = delete;
    Cable(Id<Cable> c, QObject* parent);

    template<typename T>
    Cable(T&& vis, const score::DocumentContext& ctx, QObject* parent):
      IdentifiedObject{vis, parent}
    {
      vis.writeTo(*this);
    }
    Cable(const score::DocumentContext& ctx, Id<Cable> c, const CableData& data, QObject* parent);

    void update(const score::DocumentContext& ctx, const CableData&);

    CableData toCableData() const;
    ossia::node_ptr source_node;
    ossia::node_ptr sink_node;
    ossia::outlet_ptr source_port;
    ossia::inlet_ptr sink_port;
    ossia::edge_ptr exec;

    CableType type() const;
    Process::Port* source() const;
    Process::Port* sink() const;

    void setType(CableType type);
    void setSource(Process::Port* source);
    void setSink(Process::Port* sink);

signals:
    void typeChanged(CableType type);
    void sourceChanged(Process::Port* source);
    void sinkChanged(Process::Port* sink);

private:
    CableType m_type{};
    QPointer<Process::Port> m_source{};
    QPointer<Process::Port> m_sink{};
    QMetaObject::Connection m_srcDeath, m_sinkDeath;
};


}
