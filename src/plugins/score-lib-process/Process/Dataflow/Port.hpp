#pragma once
#include <Process/Dataflow/PortType.hpp>
#include <State/Address.hpp>
#include <State/Domain.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/path/Path.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/network/value/value.hpp>

#include <score_lib_process_export.h>
#include <smallfun.hpp>

#include <QPointF>
#include <verdigris>

#if __cpp_constexpr >= 201907
#define VIRTUAL_CONSTEXPR constexpr
#else
#define VIRTUAL_CONSTEXPR
#endif

namespace ossia
{
struct inlet;
struct outlet;
}
namespace Process
{
class Port;
class Inlet;
class Outlet;
class ValueInlet;
class ValueOutlet;
class AudioInlet;
class AudioOutlet;
class MidiInlet;
class MidiOutlet;
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
    Process::ValueInlet,
    "769dd38a-bfb3-4dc6-b52a-b6abb7afe2a3")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::ValueOutlet,
    "cff96158-cc72-46d7-99dc-b6038171375b")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::MidiInlet,
    "c18adc77-e0e0-4ddf-a46c-43cb0719a890")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::MidiOutlet,
    "d8a3ed3d-b9c2-46f2-bdb3-d282a48481c6")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::AudioInlet,
    "a1574bb0-cbd4-4c7d-9417-0c25cfd1187b")
UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::AudioOutlet,
    "a1d97535-18ac-444a-8417-0cbc1692d897")
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
using Inlets = ossia::small_vector<Process::Inlet*, 4>;
using Outlets = ossia::small_vector<Process::Outlet*, 4>;
using pan_weight = ossia::small_vector<double, 2>;

// Used to know where to layout items for a given
// kind of port.
static constexpr const qreal default_margin = 5.;
static constexpr const qreal default_padding = 5.;

struct PortItemLayout
{
  QPointF port{0., 1.};
  QPointF label{12., 0.};
  QPointF control{0., 12.};
  std::optional<Qt::Alignment> labelAlignment;
  bool labelVisible{true};
};

class Cable;
class SCORE_LIB_PROCESS_EXPORT Port
    : public IdentifiedObject<Port>
    , public score::SerializableInterface<Port>
{
  W_OBJECT(Port)
  SCORE_SERIALIZE_FRIENDS
public:
  Selectable selection;
  bool hidden{};

  void addCable(const Process::Cable& c);
  void removeCable(const Path<Process::Cable>& c);
  void takeCables(Process::Port&& c);

  const QString& visualName() const noexcept;
  const QString& visualDescription() const noexcept;

  const QString& name() const noexcept;
  const State::AddressAccessor& address() const noexcept;
  const std::vector<Path<Cable>>& cables() const noexcept;
  const QString& exposed() const noexcept;
  const QString& description() const noexcept;

  virtual PortType type() const noexcept = 0;

  virtual Device::FullAddressAccessorSettings settings() const noexcept;
  virtual void setSettings(const Device::FullAddressAccessorSettings& set) noexcept;

public:
  void setName(const QString& customData);
  W_SLOT(setName);
  void setExposed(const QString& add);
  W_SLOT(setExposed);
  void setDescription(const QString& add);
  W_SLOT(setDescription);
  void setAddress(const State::AddressAccessor& address);
  W_SLOT(setAddress);

  void nameChanged(const QString& name)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, nameChanged, name)
  void exposedChanged(const QString& addr)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, exposedChanged, addr)
  void descriptionChanged(const QString& txt)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, descriptionChanged, txt)
  void cablesChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, cablesChanged)
  void addressChanged(const State::AddressAccessor& address)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, address)
  void executionReset()
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, executionReset)

  PROPERTY(
      State::AddressAccessor,
      address READ address WRITE setAddress NOTIFY addressChanged)
  PROPERTY(QString, name READ name WRITE setName NOTIFY nameChanged)

  virtual QByteArray saveData() const noexcept;
  virtual void loadData(const QByteArray& arr) noexcept;

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
  QString m_name;
  QString m_exposed;
  QString m_description;
  State::AddressAccessor m_address;
};

class SCORE_LIB_PROCESS_EXPORT Inlet : public Port
{
  W_OBJECT(Inlet)
public:
  using base_type = Inlet;
  MODEL_METADATA_IMPL_HPP(Inlet)

  ~Inlet() override;

  virtual void setupExecution(ossia::inlet&) const noexcept;
  virtual void
  forChildInlets(const smallfun::function<void(Inlet&)>&) const noexcept;
  virtual void mapExecution(
      ossia::inlet&,
      const smallfun::function<void(Inlet&, ossia::inlet&)>&) const noexcept;

protected:
  Inlet() = delete;
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
  ControlInlet() = delete;
  ~ControlInlet() override;
  ControlInlet(const ControlInlet&) = delete;
  ControlInlet(Id<Process::Port> c, QObject* parent);

  ControlInlet(DataStream::Deserializer& vis, QObject* parent);
  ControlInlet(JSONObject::Deserializer& vis, QObject* parent);
  ControlInlet(DataStream::Deserializer&& vis, QObject* parent);
  ControlInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }

  const ossia::value& value() const noexcept { return m_value; }
  const State::Domain& domain() const noexcept { return m_domain; }

  QByteArray saveData() const noexcept override;
  void loadData(const QByteArray& arr) noexcept override;

public:
  void valueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, valueChanged, v)
  void executionValueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, executionValueChanged, v)
  void domainChanged(const State::Domain& d)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, domainChanged, d)

public:
  void setValue(const ossia::value& value);
  W_SLOT(setValue)

  inline void setExecutionValue(const ossia::value& value){executionValueChanged(value);}

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
  using base_type = Outlet;
  MODEL_METADATA_IMPL_HPP(Outlet)

  ~Outlet() override;
  virtual void
  forChildInlets(const smallfun::function<void(Inlet&)>&) const noexcept;
  virtual void mapExecution(
      ossia::outlet&,
      const smallfun::function<void(Inlet&, ossia::inlet&)>&) const noexcept;

protected:
  Outlet() = delete;
  Outlet(const Outlet&) = delete;
  Outlet(Id<Process::Port> c, QObject* parent);

  Outlet(DataStream::Deserializer& vis, QObject* parent);
  Outlet(JSONObject::Deserializer& vis, QObject* parent);
  Outlet(DataStream::Deserializer&& vis, QObject* parent);
  Outlet(JSONObject::Deserializer&& vis, QObject* parent);
};

class SCORE_LIB_PROCESS_EXPORT AudioInlet : public Inlet
{
  W_OBJECT(AudioInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(AudioInlet)
  AudioInlet() = delete;
  ~AudioInlet() override;
  AudioInlet(const AudioInlet&) = delete;
  AudioInlet(Id<Process::Port> c, QObject* parent);

  AudioInlet(DataStream::Deserializer& vis, QObject* parent);
  AudioInlet(JSONObject::Deserializer& vis, QObject* parent);
  AudioInlet(DataStream::Deserializer&& vis, QObject* parent);
  AudioInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Audio;
  }
};

class SCORE_LIB_PROCESS_EXPORT AudioOutlet : public Outlet
{
  W_OBJECT(AudioOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(AudioOutlet)
  AudioOutlet() = delete;
  ~AudioOutlet() override;
  AudioOutlet(const AudioOutlet&) = delete;
  AudioOutlet(Id<Process::Port> c, QObject* parent);

  AudioOutlet(DataStream::Deserializer& vis, QObject* parent);
  AudioOutlet(JSONObject::Deserializer& vis, QObject* parent);
  AudioOutlet(DataStream::Deserializer&& vis, QObject* parent);
  AudioOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Audio;
  }

  void forChildInlets(
      const smallfun::function<void(Inlet&)>&) const noexcept override;
  void mapExecution(
      ossia::outlet&,
      const smallfun::function<void(Inlet&, ossia::inlet&)>&)
      const noexcept override;

  QByteArray saveData() const noexcept override;
  void loadData(const QByteArray& arr) noexcept override;

  bool propagate() const;
  void setPropagate(bool propagate);
  void propagateChanged(bool propagate)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, propagateChanged, propagate)

  double gain() const;
  void setGain(double g);
  void gainChanged(double g) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, gainChanged, g)

  pan_weight pan() const;
  void setPan(pan_weight g);
  void panChanged(pan_weight g)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, panChanged, g)

  std::unique_ptr<Process::ControlInlet> gainInlet;
  std::unique_ptr<Process::ControlInlet> panInlet;

  PROPERTY(
      bool,
      propagate READ propagate WRITE setPropagate NOTIFY propagateChanged)
  PROPERTY(double, gain READ gain WRITE setGain NOTIFY gainChanged)
  PROPERTY(pan_weight, pan READ pan WRITE setPan NOTIFY panChanged)
private:
  double m_gain{};
  pan_weight m_pan;
  bool m_propagate{false};
};

class SCORE_LIB_PROCESS_EXPORT MidiInlet : public Inlet
{
  W_OBJECT(MidiInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(MidiInlet)
  MidiInlet() = delete;
  ~MidiInlet() override;
  MidiInlet(const MidiInlet&) = delete;
  MidiInlet(Id<Process::Port> c, QObject* parent);

  MidiInlet(DataStream::Deserializer& vis, QObject* parent);
  MidiInlet(JSONObject::Deserializer& vis, QObject* parent);
  MidiInlet(DataStream::Deserializer&& vis, QObject* parent);
  MidiInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Midi;
  }
};

class SCORE_LIB_PROCESS_EXPORT MidiOutlet : public Outlet
{
  W_OBJECT(MidiOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(MidiOutlet)
  MidiOutlet() = delete;
  ~MidiOutlet() override;
  MidiOutlet(const MidiOutlet&) = delete;
  MidiOutlet(Id<Process::Port> c, QObject* parent);

  MidiOutlet(DataStream::Deserializer& vis, QObject* parent);
  MidiOutlet(JSONObject::Deserializer& vis, QObject* parent);
  MidiOutlet(DataStream::Deserializer&& vis, QObject* parent);
  MidiOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Midi;
  }
};

class SCORE_LIB_PROCESS_EXPORT ControlOutlet : public Outlet
{
  W_OBJECT(ControlOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(ControlOutlet)
  ControlOutlet() = delete;
  ControlOutlet(const Outlet&) = delete;
  ControlOutlet(Id<Process::Port> c, QObject* parent);
  ~ControlOutlet() override;

  ControlOutlet(DataStream::Deserializer& vis, QObject* parent);
  ControlOutlet(JSONObject::Deserializer& vis, QObject* parent);
  ControlOutlet(DataStream::Deserializer&& vis, QObject* parent);
  ControlOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }

  QByteArray saveData() const noexcept override;
  void loadData(const QByteArray& arr) noexcept override;

  const ossia::value& value() const { return m_value; }
  const State::Domain& domain() const { return m_domain; }

public:
  void valueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, valueChanged, v)
  void executionValueChanged(const ossia::value& v)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, executionValueChanged, v)
  void domainChanged(const State::Domain& d)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, domainChanged, d)

public:
  void setExecutionValue(const ossia::value& value)
  {
    executionValueChanged(value);
  }

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

class SCORE_LIB_PROCESS_EXPORT ValueInlet : public Inlet
{
  W_OBJECT(ValueInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(ValueInlet)
  ValueInlet() = delete;
  ~ValueInlet() override;
  ValueInlet(const ValueInlet&) = delete;
  ValueInlet(Id<Process::Port> c, QObject* parent);

  ValueInlet(DataStream::Deserializer& vis, QObject* parent);
  ValueInlet(JSONObject::Deserializer& vis, QObject* parent);
  ValueInlet(DataStream::Deserializer&& vis, QObject* parent);
  ValueInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }
};

class SCORE_LIB_PROCESS_EXPORT ValueOutlet : public Outlet
{
  W_OBJECT(ValueOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(ValueOutlet)
  ValueOutlet() = delete;
  ~ValueOutlet() override;
  ValueOutlet(const ValueOutlet&) = delete;
  ValueOutlet(Id<Process::Port> c, QObject* parent);

  ValueOutlet(DataStream::Deserializer& vis, QObject* parent);
  ValueOutlet(JSONObject::Deserializer& vis, QObject* parent);
  ValueOutlet(DataStream::Deserializer&& vis, QObject* parent);
  ValueOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR PortType type() const noexcept override
  {
    return Process::PortType::Message;
  }
};

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Inlet>
make_value_inlet(const Id<Process::Port>& c, QObject* parent);
SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Outlet>
make_value_outlet(const Id<Process::Port>& c, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiInlet>
make_midi_inlet(const Id<Process::Port>& c, QObject* parent);
SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiOutlet>
make_midi_outlet(const Id<Process::Port>& c, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioInlet>
make_audio_inlet(const Id<Process::Port>& c, QObject* parent);
SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioOutlet>
make_audio_outlet(const Id<Process::Port>& c, QObject* parent);

using Inlets = ossia::small_vector<Process::Inlet*, 4>;
using Outlets = ossia::small_vector<Process::Outlet*, 4>;
}

DEFAULT_MODEL_METADATA(Process::Port, "Port")

W_REGISTER_ARGTYPE(Id<Process::Port>)
W_REGISTER_ARGTYPE(Process::Port)

Q_DECLARE_METATYPE(Process::pan_weight)
W_REGISTER_ARGTYPE(Process::pan_weight)
