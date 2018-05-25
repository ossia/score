#pragma once
#include <Automation/Color/GradientAutomMetadata.hpp>
#include <wobjectdefs.h>
#include <Process/Dataflow/Port.hpp>
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

  W_OBJECT(ProcessModel)

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel() override;

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_outlets.push_back(outlet.get());
  }

  const State::AddressAccessor& address() const;

  bool tween() const;
  void setTween(bool tween);

  QString prettyName() const override;

  using gradient_colors = boost::container::flat_map<double, QColor>;
  const gradient_colors& gradient() const;
  void setGradient(const gradient_colors& c);

  std::unique_ptr<Process::Outlet> outlet;

public:
  void tweenChanged(bool tween) W_SIGNAL(tweenChanged, tween);
  void gradientChanged() W_SIGNAL(gradientChanged);

  PROPERTY(bool, tween READ tween WRITE setTween NOTIFY tweenChanged)
private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  boost::container::flat_map<double, QColor> m_colors;

  bool m_tween = false;
};
}
