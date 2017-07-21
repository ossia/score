#pragma once
#include <iscore/model/path/Path.hpp>
#include <State/Address.hpp>
#include <ossia/detail/optional.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <iscore_lib_process_export.h>

namespace QtNodes {
class Connection;
}
namespace Process
{
class DataflowProcess;
enum class CableType { ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict };

struct ISCORE_LIB_PROCESS_EXPORT CableData
{
    CableType type;
    Path<DataflowProcess> source, sink;
    ossia::optional<int> outlet, inlet;

    ISCORE_LIB_PROCESS_EXPORT
    friend bool operator==(const CableData& lhs, const CableData& rhs);
};

class ISCORE_LIB_PROCESS_EXPORT Cable
    : public IdentifiedObject<Cable>
{
    Q_OBJECT
    Q_PROPERTY(CableType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(DataflowProcess* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(DataflowProcess* sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(ossia::optional<int> outlet READ outlet WRITE setOutlet NOTIFY outletChanged)
    Q_PROPERTY(ossia::optional<int> inlet READ inlet WRITE setInlet NOTIFY inletChanged)
  public:
    Cable() = delete;
    ~Cable();
    Cable(const Cable&) = delete;
    Cable(Id<Cable> c);

    Cable(const iscore::DocumentContext& ctx, Id<Cable> c, const CableData& data);

    std::shared_ptr<ossia::graph_node> source_node;
    std::shared_ptr<ossia::graph_node> sink_node;
    std::shared_ptr<ossia::graph_edge> exec;

    CableType type() const;
    DataflowProcess* source() const;
    DataflowProcess* sink() const;
    ossia::optional<int> outlet() const;
    ossia::optional<int> inlet() const;

    void setType(CableType type);
    void setSource(DataflowProcess* source);
    void setSink(DataflowProcess* sink);
    void setOutlet(ossia::optional<int> outlet);
    void setInlet(ossia::optional<int> inlet);

signals:
    void typeChanged(CableType type);
    void sourceChanged(DataflowProcess* source);
    void sinkChanged(DataflowProcess* sink);
    void outletChanged(ossia::optional<int> outlet);
    void inletChanged(ossia::optional<int> inlet);

private:
    CableType m_type{};
    DataflowProcess* m_source{};
    DataflowProcess* m_sink{};
    ossia::optional<int> m_outlet, m_inlet;
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
