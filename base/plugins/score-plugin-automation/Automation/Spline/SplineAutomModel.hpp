#pragma once
#include <Automation/Spline/SplineAutomMetadata.hpp>
#include <Process/Process.hpp>
#include <State/Address.hpp>
#include <State/Unit.hpp>
#include <boost/container/flat_map.hpp>
#include <ossia/dataflow/nodes/spline.hpp>
#include <score_plugin_automation_export.h>

namespace Spline
{

class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Spline::ProcessModel)

  Q_OBJECT
  Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress NOTIFY addressChanged)
  Q_PROPERTY(bool tween READ tween WRITE setTween NOTIFY tweenChanged)
  Q_PROPERTY(State::Unit unit READ unit WRITE setUnit NOTIFY unitChanged)

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel();

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    // TODO m_outlets.push_back(outlet.get());
  }
  State::AddressAccessor address() const;
  void setAddress(const State::AddressAccessor& arg);
  State::Unit unit() const;
  void setUnit(const State::Unit&);

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

  QString prettyName() const override;

  const ossia::nodes::spline_data& spline() const { return m_spline; }
  void setSpline(const ossia::nodes::spline_data& c) {
    if(m_spline != c)
    {
      m_spline = c;
      splineChanged();
    }
  }
Q_SIGNALS:
  void addressChanged(const ::State::AddressAccessor&);
  void tweenChanged(bool tween);
  void unitChanged(const State::Unit&);
  void splineChanged();

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  State::AddressAccessor m_address;
  ossia::nodes::spline_data m_spline;

  bool m_tween = false;
};
}
