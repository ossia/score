#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore_plugin_interpolation_export.h>

namespace Interpolation
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Interpolation::ProcessModel,
    "aa569e11-03a9-4023-92c2-b590e88fec90",
    "Interpolation",
    "Interpolation")
namespace Interpolation
{
class ISCORE_PLUGIN_INTERPOLATION_EXPORT ProcessState final
    : public ProcessStateDataInterface
{
public:
  enum Point
  {
    Start,
    End
  };
  // watchedPoint : something between 0 and 1
  ProcessState(ProcessModel& process, Point watchedPoint, QObject* parent);

  ProcessModel& process() const;

  State::Message message() const;
  Point point() const;

  ProcessState* clone(QObject* parent) const override;

  std::vector<State::AddressAccessor> matchingAddresses() override;

  ::State::MessageList messages() const override;

  ::State::MessageList setMessages(
      const ::State::MessageList&, const Process::MessageNode&) override;

private:
  Point m_point{};
};

class ISCORE_PLUGIN_INTERPOLATION_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  ISCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Interpolation::ProcessModel)

  Q_OBJECT
  Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress
                 NOTIFY addressChanged)
  Q_PROPERTY(State::Value start READ start WRITE setStart NOTIFY startChanged)
  Q_PROPERTY(State::Value end READ end WRITE setEnd NOTIFY endChanged)

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel();

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : CurveProcessModel{vis, parent}
      , m_startState{new ProcessState{*this, ProcessState::Start, this}}
      , m_endState{new ProcessState{*this, ProcessState::End, this}}
  {
    vis.writeTo(*this);
  }

  State::AddressAccessor address() const;
  const State::Unit& sourceUnit() const;

  State::Value start() const;
  State::Value end() const;

  void setAddress(const ::State::AddressAccessor& arg);
  void setSourceUnit(const State::Unit&);
  void setStart(State::Value arg);
  void setEnd(State::Value arg);

  QString prettyName() const override;

signals:
  void addressChanged(const ::State::AddressAccessor&);
  void startChanged(const State::Value&);
  void endChanged(const State::Value&);
  void tweenChanged(bool tween);

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  /// States
  ProcessState* startStateData() const override;

  ProcessState* endStateData() const override;

  ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ::State::AddressAccessor m_address;
  State::Unit m_sourceUnit;

  State::Value m_start{};
  State::Value m_end{};

  ProcessState* m_startState{};
  ProcessState* m_endState{};
};
}
