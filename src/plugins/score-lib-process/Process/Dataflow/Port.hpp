#pragma once
#include <Process/Dataflow/PortType.hpp>
#include <State/Address.hpp>
#include <State/Domain.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/path/Path.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/network/value/value.hpp>


#include <score_lib_process_export.h>
#include <verdigris>

namespace Process
{
class Port;
class Inlet;
class Outlet;
class ControlInlet;
class ControlOutlet;
}
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::Inlet,
    "8884228a-d197-4b0a-b6ca-d1fb15291559")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::Outlet,
    "34e2c5a7-18c4-4759-b6cc-46feaeee06e2")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::ControlInlet,
    "9a13fb32-269a-47bf-99a9-930188c1f19c")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::ControlOutlet,
    "3620ea94-5991-41cf-89b3-11f842cc39d0")
namespace Process
{
class Cable;
class SCORE_LIB_PROCESS_EXPORT Port : public IdentifiedObject<Port>,
                                      public score::SerializableInterface<Port>
{
  W_OBJECT(Port)
  SCORE_SERIALIZE_FRIENDS
public:
  Selectable selection;
  PortType type{};
  bool hidden{};

  void addCable(const Path<Process::Cable>& c);
  void removeCable(const Path<Process::Cable>& c);

  const QString& customData() const;
  const State::AddressAccessor& address() const;
  const std::vector<Path<Cable>>& cables() const;
  const QString& exposed() const;
  const QString& description() const;

public:
  void setCustomData(const QString& customData);
  W_SLOT(setCustomData);
  void setExposed(const QString& add);
  W_SLOT(setExposed);
  void setDescription(const QString& add);
  W_SLOT(setDescription);
  void setAddress(const State::AddressAccessor& address);
  W_SLOT(setAddress);

public:
  void exposedChanged(const QString& addr)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, exposedChanged, addr)
  void descriptionChanged(const QString& txt)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, descriptionChanged, txt)
  void cablesChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, cablesChanged)
  void customDataChanged(const QString& customData)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, customDataChanged, customData)
  void addressChanged(const State::AddressAccessor& address)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, address)

  PROPERTY(
      State::AddressAccessor,
      address READ address WRITE setAddress NOTIFY addressChanged)
  PROPERTY(
      QString,
      customData READ customData WRITE setCustomData NOTIFY customDataChanged)
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
  QString m_exposed;
  QString m_description;
  State::AddressAccessor m_address;
};

class SCORE_LIB_PROCESS_EXPORT Inlet : public Port
{
  W_OBJECT(Inlet)
public:
  MODEL_METADATA_IMPL_HPP(Inlet)
  Inlet() = delete;
  ~Inlet() override;
  Inlet(const Inlet&) = delete;
  Inlet(Id<Process::Port> c, QObject* parent);

  Inlet(DataStream::Deserializer& vis, QObject* parent);
  Inlet(JSONObject::Deserializer& vis, QObject* parent);
  Inlet(DataStream::Deserializer&& vis, QObject* parent);
  Inlet(JSONObject::Deserializer&& vis, QObject* parent);
};

class SCORE_LIB_PROCESS_EXPORT ControlInlet : public Inlet
{
  W_OBJECT(ControlInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(ControlInlet)
  using Inlet::Inlet;
  ~ControlInlet() override;

  ControlInlet(DataStream::Deserializer& vis, QObject* parent);
  ControlInlet(JSONObject::Deserializer& vis, QObject* parent);
  ControlInlet(DataStream::Deserializer&& vis, QObject* parent);
  ControlInlet(JSONObject::Deserializer&& vis, QObject* parent);

  const ossia::value& value() const noexcept { return m_value; }
  const State::Domain& domain() const noexcept { return m_domain; }

public:
  void valueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, valueChanged, v)
  void domainChanged(const State::Domain& d)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, domainChanged, d)

public:
  void setValue(const ossia::value& value)
  {
    if (value != m_value)
    {
      m_value = value;
      valueChanged(value);
    }
  }
  W_SLOT(setValue)

  void setDomain(const State::Domain& d)
  {
    if (m_domain != d)
    {
      m_domain = d;
      domainChanged(d);
    }
  }
  W_SLOT(setDomain)

  PROPERTY(
      State::Domain,
      domain READ domain WRITE setDomain NOTIFY domainChanged)
  PROPERTY(ossia::value, value READ value WRITE setValue NOTIFY valueChanged)
private:
  ossia::value m_value;
  State::Domain m_domain;
};

class SCORE_LIB_PROCESS_EXPORT Outlet : public Port
{
  W_OBJECT(Outlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(Outlet)
  Outlet() = delete;
  ~Outlet() override;
  Outlet(const Outlet&) = delete;
  Outlet(Id<Process::Port> c, QObject* parent);

  Outlet(DataStream::Deserializer& vis, QObject* parent);
  Outlet(JSONObject::Deserializer& vis, QObject* parent);
  Outlet(DataStream::Deserializer&& vis, QObject* parent);
  Outlet(JSONObject::Deserializer&& vis, QObject* parent);

  bool propagate() const;

public:
  void propagateChanged(bool propagate)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, propagateChanged, propagate)

public:
  void setPropagate(bool propagate);
  W_SLOT(setPropagate);

private:
  bool m_propagate{false};

  W_PROPERTY(
      bool,
      propagate READ propagate WRITE setPropagate NOTIFY propagateChanged)
};

class SCORE_LIB_PROCESS_EXPORT ControlOutlet final : public Outlet
{
  W_OBJECT(ControlOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(ControlOutlet)
  using Outlet::Outlet;
  ~ControlOutlet() override;

  ControlOutlet(DataStream::Deserializer& vis, QObject* parent);
  ControlOutlet(JSONObject::Deserializer& vis, QObject* parent);
  ControlOutlet(DataStream::Deserializer&& vis, QObject* parent);
  ControlOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  const ossia::value& value() const { return m_value; }
  const State::Domain& domain() const { return m_domain; }

public:
  void valueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, valueChanged, v)
  void domainChanged(const State::Domain& d)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, domainChanged, d)

public:
  void setValue(const ossia::value& value)
  {
    if (value != m_value)
    {
      m_value = value;
      valueChanged(value);
    }
  }
  W_SLOT(setValue)

  void setDomain(const State::Domain& d)
  {
    if (m_domain != d)
    {
      m_domain = d;
      domainChanged(d);
    }
  }
  W_SLOT(setDomain)

  PROPERTY(
      State::Domain,
      domain READ domain WRITE setDomain NOTIFY domainChanged)
  PROPERTY(ossia::value, value READ value WRITE setValue NOTIFY valueChanged)
private:
  ossia::value m_value;
  State::Domain m_domain;
};

inline std::unique_ptr<Inlet>
make_inlet(const Id<Process::Port>& c, QObject* parent)
{
  return std::make_unique<Inlet>(c, parent);
}

inline std::unique_ptr<Outlet>
make_outlet(const Id<Process::Port>& c, QObject* parent)
{
  return std::make_unique<Outlet>(c, parent);
}

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

W_REGISTER_ARGTYPE(Id<Process::Port>)
W_REGISTER_ARGTYPE(Process::Port)
