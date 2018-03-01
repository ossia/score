#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score_plugin_scenario_export.h>
namespace Interpolation
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Interpolation::ProcessModel,
    "aa569e11-03a9-4023-92c2-b590e88fec90",
    "Interpolation",
    "Interpolation",
    "Automations",
    (QStringList{"Curve", "Automation"}),
    Process::ProcessFlags::SupportsTemporal)
namespace Interpolation
{
class ProcessState final
    : public ProcessStateDataInterface
{
    Q_OBJECT
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

  std::vector<State::AddressAccessor> matchingAddresses() override;

  ::State::MessageList messages() const override;

  ::State::MessageList setMessages(
      const ::State::MessageList&, const Process::MessageNode&) override;

private:
  Point m_point{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Interpolation::ProcessModel)

  Q_OBJECT
  Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress
                 NOTIFY addressChanged)
  Q_PROPERTY(ossia::value start READ start WRITE setStart NOTIFY startChanged)
  Q_PROPERTY(ossia::value end READ end WRITE setEnd NOTIFY endChanged)
  Q_PROPERTY(bool tween READ tween WRITE setTween NOTIFY tweenChanged)

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

  ossia::value start() const;
  ossia::value end() const;

  void setAddress(const ::State::AddressAccessor& arg);
  void setSourceUnit(const State::Unit&);
  void setStart(ossia::value arg);
  void setEnd(ossia::value arg);

  QString prettyName() const override;

  bool tween() const
  {
    return m_tween;
  }
  void setTween(bool tween)
  {
    if (m_tween == tween)
      return;

    m_tween = tween;
    tweenChanged(tween);
  }

Q_SIGNALS:
  void addressChanged(const ::State::AddressAccessor&);
  void startChanged(const ossia::value&);
  void endChanged(const ossia::value&);
  void tweenChanged(bool tween);

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;
  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  /// States
  ProcessState* startStateData() const override;
  ProcessState* endStateData() const override;

  ::State::AddressAccessor m_address;
  State::Unit m_sourceUnit;

  ossia::value m_start{};
  ossia::value m_end{};

  ProcessState* m_startState{};
  ProcessState* m_endState{};
  bool m_tween = false;
};
}
