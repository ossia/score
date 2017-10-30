#pragma once
#include <score/model/path/Path.hpp>
#include <State/Address.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <QPointer>
#include <score_lib_process_export.h>

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
    Q_PROPERTY(bool propagate READ propagate WRITE setPropagate NOTIFY propagateChanged)
    SCORE_SERIALIZE_FRIENDS
  public:
    PortType type{};
    bool outlet{false};

    Port() = delete;
    ~Port();
    Port(const Port&) = delete;
    Port(Id<Port> c, QObject* parent);
    Port(Id<Port> c, const Port& other, QObject* parent);

    Port* clone(QObject* parent) const;

    Port(DataStream::Deserializer& vis, QObject* parent);
    Port(JSONObject::Deserializer& vis, QObject* parent);
    Port(DataStream::Deserializer&& vis, QObject* parent);
    Port(JSONObject::Deserializer&& vis, QObject* parent);

    void addCable(const Path<Process::Cable>& c);
    void removeCable(const Path<Process::Cable>& c);

    QString customData() const;

    State::AddressAccessor address() const;
    const std::vector<Path<Cable>>& cables() const;

    bool propagate() const;

public slots:
    void setCustomData(const QString& customData);
    void setAddress(const State::AddressAccessor& address);
    void setPropagate(bool propagate);

signals:
    void cablesChanged();
    void customDataChanged(const QString& customData);
    void addressChanged(const State::AddressAccessor& address);
    void propagateChanged(bool propagate);

private:
    std::vector<Path<Cable>> m_cables;
    QString m_customData;
    State::AddressAccessor m_address;
    bool m_propagate{false};
};
}
