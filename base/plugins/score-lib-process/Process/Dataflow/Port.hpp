#pragma once
#include <score/model/path/Path.hpp>
#include <State/Address.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <QPointer>
#include <chobo/small_vector.hpp>
#include <score_lib_process_export.h>
#include <ossia/network/value/value.hpp>
#include <State/Domain.hpp>
namespace Process
{
class Cable;
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
      bool hidden{};

    virtual Port* clone(QObject* parent) const;
    void addCable(const Path<Process::Cable>& c);
    void removeCable(const Path<Process::Cable>& c);

    const QString& customData() const;

    const State::AddressAccessor& address() const;
    const std::vector<Path<Cable>>& cables() const;


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
    Port(Id<Port> c, const Port& other, QObject* parent);


    Port(DataStream::Deserializer& vis, QObject* parent);
    Port(JSONObject::Deserializer& vis, QObject* parent);
    Port(DataStream::Deserializer&& vis, QObject* parent);
    Port(JSONObject::Deserializer&& vis, QObject* parent);

  private:
    std::vector<Path<Cable>> m_cables;
    QString m_customData;
    State::AddressAccessor m_address;
};

class SCORE_LIB_PROCESS_EXPORT Inlet : public Port
{
  public:
    Inlet() = delete;
    ~Inlet() override;
    Inlet(const Inlet&) = delete;
    Inlet(Id<Process::Port> c, QObject* parent);
    Inlet(Id<Process::Port> c, const Inlet& other, QObject* parent);

    Inlet* clone(QObject* parent) const override;

    Inlet(DataStream::Deserializer& vis, QObject* parent);
    Inlet(JSONObject::Deserializer& vis, QObject* parent);
    Inlet(DataStream::Deserializer&& vis, QObject* parent);
    Inlet(JSONObject::Deserializer&& vis, QObject* parent);

  private:
};

class SCORE_LIB_PROCESS_EXPORT ControlInlet final : public Inlet
{
    Q_OBJECT
    Q_PROPERTY(ossia::value value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(State::Domain domain READ domain WRITE setDomain NOTIFY domainChanged)
    Q_PROPERTY(bool uiVisible READ uiVisible WRITE setUiVisible NOTIFY uiVisibleChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      using Inlet::Inlet;
    ~ControlInlet() override;
    ControlInlet* clone(QObject* parent) const override;

    ControlInlet(Id<Port> c, const ControlInlet& other, QObject* parent);
    ControlInlet(DataStream::Deserializer& vis, QObject* parent);
    ControlInlet(JSONObject::Deserializer& vis, QObject* parent);
    ControlInlet(DataStream::Deserializer&& vis, QObject* parent);
    ControlInlet(JSONObject::Deserializer&& vis, QObject* parent);

    const ossia::value& value() const { return m_value; }
    const State::Domain& domain() const { return m_domain; }
    bool uiVisible() const { return m_ui; }

  signals:
    void valueChanged(const ossia::value& v);
    void domainChanged(const State::Domain& d);
    void uiVisibleChanged(bool);

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

    void setUiVisible(bool b)
    {
      if(b != m_ui)
      {
        m_ui = b;
        emit uiVisibleChanged(b);
      }
    }
  private:
    ossia::value m_value;
    State::Domain m_domain;
    bool m_ui{false};
};

class SCORE_LIB_PROCESS_EXPORT Outlet : public Port
{
    Q_OBJECT
    Q_PROPERTY(bool propagate READ propagate WRITE setPropagate NOTIFY propagateChanged)
    SCORE_SERIALIZE_FRIENDS
    public:
      Outlet() = delete;
    ~Outlet() override;
    Outlet(const Outlet&) = delete;
    Outlet(Id<Process::Port> c, QObject* parent);
    Outlet(Id<Process::Port> c, const Outlet& other, QObject* parent);

    Outlet* clone(QObject* parent) const override;

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
      using Outlet::Outlet;
    ~ControlOutlet() override;
    ControlOutlet* clone(QObject* parent) const override;

    ControlOutlet(Id<Port> c, const ControlOutlet& other, QObject* parent);
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

template<typename... Args>
inline std::unique_ptr<Inlet> make_inlet(Args&&... args)
{ return std::make_unique<Inlet>(std::forward<Args>(args)...); }
template<typename... Args>
inline std::unique_ptr<ControlInlet> make_control(Args&&... args)
{ return std::make_unique<ControlInlet>(std::forward<Args>(args)...); }
template<typename... Args>
inline std::unique_ptr<ControlInlet> make_control_out(Args&&... args)
{ return std::make_unique<ControlOutlet>(std::forward<Args>(args)...); }
template<typename... Args>
inline std::unique_ptr<Outlet> make_outlet(Args&&... args)
{ return std::make_unique<Outlet>(std::forward<Args>(args)...); }

inline std::unique_ptr<Inlet> clone_inlet(Inlet& source, QObject* parent)
{ return std::make_unique<Inlet>(source.id(), source, parent); }
inline std::unique_ptr<ControlInlet> clone_control(ControlInlet& source, QObject* parent)
{ return std::make_unique<ControlInlet>(source.id(), source, parent); }
inline std::unique_ptr<ControlOutlet> clone_control_out(ControlOutlet& source, QObject* parent)
{ return std::make_unique<ControlOutlet>(source.id(), source, parent); }
inline std::unique_ptr<Outlet> clone_outlet(Outlet& source, QObject* parent)
{ return std::make_unique<Outlet>(source.id(), source, parent); }


using Inlets = chobo::small_vector<Process::Inlet*, 4>;
using Outlets = chobo::small_vector<Process::Outlet*, 4>;
}

DEFAULT_MODEL_METADATA(Process::Port, "Port")
