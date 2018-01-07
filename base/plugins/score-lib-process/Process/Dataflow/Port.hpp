#pragma once
#include <score/model/path/Path.hpp>
#include <State/Address.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score_lib_process_export.h>
#include <State/Domain.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/detail/small_vector.hpp>
#include <QUuid>
#include <QPointer>

namespace Process
{
class Port;
class Inlet;
class Outlet;
class ControlInlet;
class ControlOutlet;
}
UUID_METADATA(SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Inlet, "8884228a-d197-4b0a-b6ca-d1fb15291559")
UUID_METADATA(SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::Outlet, "34e2c5a7-18c4-4759-b6cc-46feaeee06e2")
UUID_METADATA(SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ControlInlet, "9a13fb32-269a-47bf-99a9-930188c1f19c")
UUID_METADATA(SCORE_LIB_PROCESS_EXPORT, Process::Port, Process::ControlOutlet, "3620ea94-5991-41cf-89b3-11f842cc39d0")
namespace Process
{
class Cable;
enum class PortType { Message, Audio, Midi };
class SCORE_LIB_PROCESS_EXPORT Port
    : public IdentifiedObject<Port>
    , public score::SerializableInterface<Port>
{
    Q_OBJECT
    Q_PROPERTY(QString customData READ customData WRITE setCustomData NOTIFY customDataChanged)
    Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress NOTIFY addressChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      PortType type{};
      bool hidden{};

    void addCable(const Path<Process::Cable>& c);
    void removeCable(const Path<Process::Cable>& c);

    const QString& customData() const;
    const State::AddressAccessor& address() const;
    const std::vector<Path<Cable>>& cables() const;

    const score::Components& components() const
    { return m_components; }
    score::Components& components()
    { return m_components; }

    const QUuid& uuid() const { return m_uuid; }

  public slots:
    void setCustomData(const QString& customData);
    void setAddress(const State::AddressAccessor& address);

  signals:
    void cablesChanged();
    void customDataChanged(const QString& customData);
    void addressChanged(const State::AddressAccessor& address);

  protected:
    Port() = delete;
    ~Port() override;
    Port(const Port&) = delete;
    Port(Id<Port> c, const QString& name, QObject* parent);


    Port(DataStream::Deserializer& vis, QObject* parent);
    Port(JSONObject::Deserializer& vis, QObject* parent);
    Port(DataStream::Deserializer&& vis, QObject* parent);
    Port(JSONObject::Deserializer&& vis, QObject* parent);

  private:
    std::vector<Path<Cable>> m_cables;
    QString m_customData;
    State::AddressAccessor m_address;
    score::Components m_components;
    QUuid m_uuid;
};

class SCORE_LIB_PROCESS_EXPORT Inlet : public Port
{
  public:
    MODEL_METADATA_IMPL(Inlet)
    Inlet() = delete;
    ~Inlet() override;
    Inlet(const Inlet&) = delete;
    Inlet(Id<Process::Port> c, QObject* parent);

    Inlet(DataStream::Deserializer& vis, QObject* parent);
    Inlet(JSONObject::Deserializer& vis, QObject* parent);
    Inlet(DataStream::Deserializer&& vis, QObject* parent);
    Inlet(JSONObject::Deserializer&& vis, QObject* parent);

  private:
};

class SCORE_LIB_PROCESS_EXPORT ControlInlet : public Inlet
{
    Q_OBJECT
    Q_PROPERTY(ossia::value value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(State::Domain domain READ domain WRITE setDomain NOTIFY domainChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      MODEL_METADATA_IMPL(ControlInlet)
      using Inlet::Inlet;
    ~ControlInlet() override;

    ControlInlet(DataStream::Deserializer& vis, QObject* parent);
    ControlInlet(JSONObject::Deserializer& vis, QObject* parent);
    ControlInlet(DataStream::Deserializer&& vis, QObject* parent);
    ControlInlet(JSONObject::Deserializer&& vis, QObject* parent);

    const ossia::value& value() const { return m_value; }
    const State::Domain& domain() const { return m_domain; }

  signals:
    void valueChanged(const ossia::value& v);
    void domainChanged(const State::Domain& d);

  public slots:
    void setValue(const ossia::value& value)
    {
      if(value != m_value)
      {
        m_value = value;
        emit valueChanged(value);
      }
    }

    void setDomain(const State::Domain& d)
    {
      if(m_domain != d)
      {
        m_domain = d;
        emit domainChanged(d);
      }
    }
  private:
    ossia::value m_value;
    State::Domain m_domain;
};

class SCORE_LIB_PROCESS_EXPORT Outlet : public Port
{
    Q_OBJECT
    Q_PROPERTY(bool propagate READ propagate WRITE setPropagate NOTIFY propagateChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      MODEL_METADATA_IMPL(Outlet)
      Outlet() = delete;
    ~Outlet() override;
    Outlet(const Outlet&) = delete;
    Outlet(Id<Process::Port> c, QObject* parent);

    Outlet(DataStream::Deserializer& vis, QObject* parent);
    Outlet(JSONObject::Deserializer& vis, QObject* parent);
    Outlet(DataStream::Deserializer&& vis, QObject* parent);
    Outlet(JSONObject::Deserializer&& vis, QObject* parent);


    bool propagate() const;

  signals:
    void propagateChanged(bool propagate);

  public slots:
    void setPropagate(bool propagate);

  private:
    bool m_propagate{false};
};

class SCORE_LIB_PROCESS_EXPORT ControlOutlet final : public Outlet
{
    Q_OBJECT
    Q_PROPERTY(ossia::value value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(State::Domain domain READ domain WRITE setDomain NOTIFY domainChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      MODEL_METADATA_IMPL(ControlOutlet)
      using Outlet::Outlet;
    ~ControlOutlet() override;

    ControlOutlet(DataStream::Deserializer& vis, QObject* parent);
    ControlOutlet(JSONObject::Deserializer& vis, QObject* parent);
    ControlOutlet(DataStream::Deserializer&& vis, QObject* parent);
    ControlOutlet(JSONObject::Deserializer&& vis, QObject* parent);

    const ossia::value& value() const { return m_value; }
    const State::Domain& domain() const { return m_domain; }

  signals:
    void valueChanged(const ossia::value& v);
    void domainChanged(const State::Domain& d);

  public slots:
    void setValue(const ossia::value& value)
    {
      if(value != m_value)
      {
        m_value = value;
        emit valueChanged(value);
      }
    }

    void setDomain(const State::Domain& d)
    {
      if(m_domain != d)
      {
        m_domain = d;
        emit domainChanged(d);
      }
    }

  private:
    ossia::value m_value;
    State::Domain m_domain;
};

inline std::unique_ptr<Inlet> make_inlet(const Id<Process::Port>& c, QObject* parent)
{ return std::make_unique<Inlet>(c, parent); }

inline std::unique_ptr<Outlet> make_outlet(const Id<Process::Port>& c, QObject* parent)
{ return std::make_unique<Outlet>(c, parent); }

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Inlet> make_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Inlet> make_inlet(JSONObjectWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Outlet> make_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Outlet> make_outlet(JSONObjectWriter& wr, QObject* parent);


using Inlets = ossia::small_vector<Process::Inlet*, 4>;
using Outlets = ossia::small_vector<Process::Outlet*, 4>;
}

DEFAULT_MODEL_METADATA(Process::Port, "Port")
