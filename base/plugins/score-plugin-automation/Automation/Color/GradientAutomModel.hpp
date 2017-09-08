#pragma once
#include <Automation/Color/GradientAutomMetadata.hpp>
#include <Process/Process.hpp>
#include <State/Address.hpp>
#include <State/Unit.hpp>
#include <boost/container/flat_map.hpp>
#include <score_plugin_automation_export.h>

namespace Gradient
{

class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gradient::ProcessModel)

  Q_OBJECT
  Q_PROPERTY(State::AddressAccessor address READ address WRITE setAddress
                 NOTIFY addressChanged)
  Q_PROPERTY(bool tween READ tween WRITE setTween NOTIFY tweenChanged)

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
  }

  State::AddressAccessor address() const;
  void setAddress(const State::AddressAccessor& arg);

  bool tween() const
  {
    return m_tween;
  }
  void setTween(bool tween)
  {
    if (m_tween == tween)
      return;

    m_tween = tween;
    emit tweenChanged(tween);
  }

  QString prettyName() const override;

  using gradient_colors = boost::container::flat_map<double, QColor>;
  const gradient_colors& gradient() const { return m_colors; }
  void setGradient(const gradient_colors& c) {
    if(m_colors != c)
    {
      m_colors = c;
      emit gradientChanged();
    }
  }
signals:
  void addressChanged(const ::State::AddressAccessor&);
  void tweenChanged(bool tween);
  void gradientChanged();

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  State::AddressAccessor m_address;
  boost::container::flat_map<double, QColor> m_colors;

  bool m_tween = false;
};
}
