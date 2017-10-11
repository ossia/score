#pragma once
#include <score/model/path/Path.hpp>
#include <State/Address.hpp>
#include <ossia/detail/optional.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Component.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score_lib_process_export.h>

namespace ossia
{
class graph_node;
class graph_edge;
using node_ptr = std::shared_ptr<graph_node>;
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
struct Port;
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
    Process::Port* m_source{};
    Process::Port* m_sink{};
    QMetaObject::Connection m_srcDeath, m_sinkDeath;
};

enum class PortType { Message, Audio, Midi };
class SCORE_LIB_PROCESS_EXPORT Port
    : public IdentifiedObject<Port>
{
    Q_OBJECT
    Q_PROPERTY(QString customData READ customData WRITE setCustomData NOTIFY customDataChanged)
    Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress NOTIFY addressChanged)
    SCORE_SERIALIZE_FRIENDS
  public:
    PortType type{};
    int num{};
    bool propagate{false};
    bool outlet{false};

    Port() = delete;
    ~Port();
    Port(const Port&) = delete;
    Port(Id<Port> c, QObject* parent);
    Port(Id<Port> c, const Port& other, QObject* parent);

    Port* clone(QObject* parent) const;

    template <typename DeserializerVisitor>
    Port(DeserializerVisitor&& vis, QObject* parent) : IdentifiedObject{vis, parent}
    {
      vis.writeTo(*this);
    }

    void addCable(const Id<Process::Cable>& c)
    {
      m_cables.push_back(c);
      emit cablesChanged();
    }
    void removeCable(const Id<Process::Cable>& c)
    {
      auto it = ossia::find(m_cables, c);
      if(it != m_cables.end())
      {
        m_cables.erase(it);
        emit cablesChanged();
      }
    }

    QString customData() const
    {
      return m_customData;
    }

    State::AddressAccessor address() const
    {
      return m_address;
    }

public slots:
    void setCustomData(const QString& customData)
    {
      if (m_customData == customData)
        return;

      m_customData = customData;
      emit customDataChanged(m_customData);
    }

    void setAddress(const State::AddressAccessor& address)
    {
      if (m_address == address)
        return;

      m_address = address;
      emit addressChanged(m_address);
    }

signals:
    void cablesChanged();
    void customDataChanged(const QString& customData);
    void addressChanged(const State::AddressAccessor& address);

private:
    std::vector<Id<Cable>> m_cables;
    QString m_customData;
    State::AddressAccessor m_address;
};

}
