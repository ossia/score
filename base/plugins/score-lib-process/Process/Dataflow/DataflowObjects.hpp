#pragma once
#include <score/model/path/Path.hpp>
#include <State/Address.hpp>
#include <ossia/detail/optional.hpp>
#include <Process/ProcessComponent.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <score/model/Component.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score_lib_process_export.h>

namespace Dataflow
{
class NodeItem;
class DocumentPlugin;
}
namespace Process
{
class DataflowProcess;
class Node;
struct Port;
class Cable;
enum class CableType { ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict };

struct SCORE_LIB_PROCESS_EXPORT CableData
{
    CableType type{};
    Path<Process::Node> source, sink;
    ossia::optional<int> outlet, inlet;

    SCORE_LIB_PROCESS_EXPORT
    friend bool operator==(const CableData& lhs, const CableData& rhs);
};

class SCORE_LIB_PROCESS_EXPORT Node
    : public score::Entity<Node>
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
  public:
    ossia::node_ptr exec{};
    QObject* ui{};

    Node(Id<Node> c, QString name, QObject* parent);
    Node(Id<Node> c, QObject* parent);

    ~Node();

    void cleanup();
    virtual QString getText() const = 0;

    virtual std::size_t audioInlets() const = 0;
    virtual std::size_t messageInlets() const = 0;
    virtual std::size_t midiInlets() const = 0;

    virtual std::size_t audioOutlets() const = 0;
    virtual std::size_t messageOutlets() const = 0;
    virtual std::size_t midiOutlets() const = 0;

    virtual std::vector<Process::Port> inlets() const = 0;
    virtual std::vector<Process::Port> outlets() const = 0;

    virtual std::vector<Id<Process::Cable>> cables() const = 0;
    virtual void addCable(Id<Process::Cable> c) = 0;
    virtual void removeCable(Id<Process::Cable> c) = 0;

    QPointF position() const
    {
      return m_position;
    }

  public slots:
    void setPosition(QPointF position)
    {
      if (m_position == position)
        return;

      m_position = position;
      emit positionChanged(m_position);
    }

  signals:
    void positionChanged(QPointF position);
    void inletsChanged();
    void outletsChanged();

  private:
    QPointF m_position;
};

class SCORE_LIB_PROCESS_EXPORT Cable
    : public IdentifiedObject<Cable>
{
    Q_OBJECT
    Q_PROPERTY(CableType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(Process::Node* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Process::Node* sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(ossia::optional<int> outlet READ outlet WRITE setOutlet NOTIFY outletChanged)
    Q_PROPERTY(ossia::optional<int> inlet READ inlet WRITE setInlet NOTIFY inletChanged)
  public:
    Cable() = delete;
    ~Cable();
    Cable(const Cable&) = delete;
    Cable(Id<Cable> c);

    Cable(const score::DocumentContext& ctx, Id<Cable> c, const CableData& data);

    void update(const score::DocumentContext& ctx, const CableData&);

    CableData toCableData() const;
    ossia::node_ptr source_node;
    ossia::node_ptr sink_node;
    ossia::edge_ptr exec;

    CableType type() const;
    Process::Node* source() const;
    Process::Node* sink() const;
    ossia::optional<int> outlet() const;
    ossia::optional<int> inlet() const;

    void setType(CableType type);
    void setSource(Process::Node* source);
    void setSink(Process::Node* sink);
    void setOutlet(ossia::optional<int> outlet);
    void setInlet(ossia::optional<int> inlet);

signals:
    void typeChanged(CableType type);
    void sourceChanged(Process::Node* source);
    void sinkChanged(Process::Node* sink);
    void outletChanged(ossia::optional<int> outlet);
    void inletChanged(ossia::optional<int> inlet);

private:
    CableType m_type{};
    Process::Node* m_source{};
    Process::Node* m_sink{};
    ossia::optional<int> m_outlet, m_inlet;
    QMetaObject::Connection m_srcDeath, m_sinkDeath;
};

enum class PortType { Message, Audio, Midi };
struct Port
{
    PortType type;
    QString customData;
    State::AddressAccessor address;
    bool propagate{false};

    friend bool operator==(const Port& lhs, const Port& rhs)
    {
      return lhs.type == rhs.type && lhs.customData == rhs.customData && lhs.address == rhs.address;
    }
};

}


namespace Dataflow
{
class NodeItem;
class DocumentPlugin;
class SCORE_LIB_PROCESS_EXPORT ProcessComponent :
    public Process::GenericProcessComponent<DocumentPlugin>
{
    Q_OBJECT
       ABSTRACT_COMPONENT_METADATA(Dataflow::ProcessComponent, "44f68a30-4bb1-4d94-940f-074f5b5b78fe")
  public:
    static const constexpr bool is_unique = true;

    ProcessComponent(
        Process::ProcessModel& process,
        DocumentPlugin& doc,
        const Id<score::Component>& id,
        const QString& name,
        QObject* parent);

    ~ProcessComponent();
};

template<typename Process_T>
using ProcessComponent_T = Process::GenericProcessComponent_T<ProcessComponent, Process_T>;


class SCORE_LIB_PROCESS_EXPORT ProcessComponentFactory :
    public score::GenericComponentFactory<
    Process::ProcessModel,
    DocumentPlugin,
    ProcessComponentFactory>
{
    SCORE_ABSTRACT_COMPONENT_FACTORY(Dataflow::ProcessComponent)
    public:
      virtual ~ProcessComponentFactory()
    {

    }

    virtual ProcessComponent* make(
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<score::Component>&,
        QObject* paren_objt) const = 0;
};

template<
    typename ProcessComponent_T>
class ProcessComponentFactory_T :
    public score::GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
  public:
    using model_type = typename ProcessComponent_T::model_type;
    ProcessComponent* make(
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<score::Component>& id,
        QObject* paren_objt) const final override
    {
      return new ProcessComponent_T{static_cast<model_type&>(proc), doc, id, paren_objt};
    }
};

using ProcessComponentFactoryList =
score::GenericComponentFactoryList<
Process::ProcessModel,
DocumentPlugin,
ProcessComponentFactory>;


}
