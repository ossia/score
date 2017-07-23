#pragma once
#include <iscore/model/path/Path.hpp>
#include <State/Address.hpp>
#include <ossia/detail/optional.hpp>
#include <Process/ProcessComponent.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <iscore/model/Component.hpp>
#include <iscore/model/ComponentHierarchy.hpp>
#include <iscore/plugins/customfactory/ModelFactory.hpp>
#include <iscore/model/ComponentFactory.hpp>
#include <iscore_lib_process_export.h>

namespace Dataflow
{
class NodeItem;
class DocumentPlugin;
class ProcessComponent;
}
namespace Process
{
class DataflowProcess;
enum class CableType { ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict };

struct ISCORE_LIB_PROCESS_EXPORT CableData
{
    CableType type{};
    Path<Dataflow::ProcessComponent> source, sink;
    ossia::optional<int> outlet, inlet;

    ISCORE_LIB_PROCESS_EXPORT
    friend bool operator==(const CableData& lhs, const CableData& rhs);
};

class ISCORE_LIB_PROCESS_EXPORT Cable
    : public IdentifiedObject<Cable>
{
    Q_OBJECT
    Q_PROPERTY(CableType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(Dataflow::ProcessComponent* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Dataflow::ProcessComponent* sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(ossia::optional<int> outlet READ outlet WRITE setOutlet NOTIFY outletChanged)
    Q_PROPERTY(ossia::optional<int> inlet READ inlet WRITE setInlet NOTIFY inletChanged)
  public:
    Cable() = delete;
    ~Cable();
    Cable(const Cable&) = delete;
    Cable(Id<Cable> c);

    Cable(const iscore::DocumentContext& ctx, Id<Cable> c, const CableData& data);

    void update(const iscore::DocumentContext& ctx, const CableData&);

    CableData toCableData() const;
    std::shared_ptr<ossia::graph_node> source_node;
    std::shared_ptr<ossia::graph_node> sink_node;
    std::shared_ptr<ossia::graph_edge> exec;

    CableType type() const;
    Dataflow::ProcessComponent* source() const;
    Dataflow::ProcessComponent* sink() const;
    ossia::optional<int> outlet() const;
    ossia::optional<int> inlet() const;

    void setType(CableType type);
    void setSource(Dataflow::ProcessComponent* source);
    void setSink(Dataflow::ProcessComponent* sink);
    void setOutlet(ossia::optional<int> outlet);
    void setInlet(ossia::optional<int> inlet);

signals:
    void typeChanged(CableType type);
    void sourceChanged(Dataflow::ProcessComponent* source);
    void sinkChanged(Dataflow::ProcessComponent* sink);
    void outletChanged(ossia::optional<int> outlet);
    void inletChanged(ossia::optional<int> inlet);

private:
    CableType m_type{};
    Dataflow::ProcessComponent* m_source{};
    Dataflow::ProcessComponent* m_sink{};
    ossia::optional<int> m_outlet, m_inlet;
    QMetaObject::Connection m_srcDeath, m_sinkDeath;
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


namespace Dataflow
{
class NodeItem;
class DocumentPlugin;
class ISCORE_LIB_PROCESS_EXPORT ProcessComponent :
    public Process::GenericProcessComponent<DocumentPlugin>
{
    Q_OBJECT
       ABSTRACT_COMPONENT_METADATA(Dataflow::ProcessComponent, "44f68a30-4bb1-4d94-940f-074f5b5b78fe")
  public:
    static const constexpr bool is_unique = true;

    ossia::node_ptr exec{};
    QObject* ui{};
    ProcessComponent(
        Process::ProcessModel& process,
        DocumentPlugin& doc,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent);

    ~ProcessComponent();
    void cleanup();

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

  signals:
    void inletsChanged();
    void outletsChanged();

};

template<typename Process_T>
using ProcessComponent_T = Process::GenericProcessComponent_T<ProcessComponent, Process_T>;


class ISCORE_LIB_PROCESS_EXPORT ProcessComponentFactory :
    public iscore::GenericComponentFactory<
    Process::ProcessModel,
    DocumentPlugin,
    ProcessComponentFactory>
{
    ISCORE_ABSTRACT_COMPONENT_FACTORY(Dataflow::ProcessComponent)
    public:
      virtual ~ProcessComponentFactory()
    {

    }

    virtual ProcessComponent* make(
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<iscore::Component>&,
        QObject* paren_objt) const = 0;
};

template<
    typename ProcessComponent_T>
class ProcessComponentFactory_T :
    public iscore::GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
  public:
    using model_type = typename ProcessComponent_T::model_type;
    ProcessComponent* make(
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<iscore::Component>& id,
        QObject* paren_objt) const final override
    {
      return new ProcessComponent_T{static_cast<model_type&>(proc), doc, id, paren_objt};
    }
};

using ProcessComponentFactoryList =
iscore::GenericComponentFactoryList<
Process::ProcessModel,
DocumentPlugin,
ProcessComponentFactory>;


}
