#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Scenario
{

template <typename Component_T>
class IntervalComponent : public Component_T
{
public:
  template <typename... Args>
  IntervalComponent(Scenario::IntervalModel& cst, Args&&... args)
      : Component_T{std::forward<Args>(args)...}, m_interval{&cst}
  {
  }

  Scenario::IntervalModel& interval() const
  {
    SCORE_ASSERT(m_interval);
    return *m_interval;
  }

  auto& context() const { return this->system().context(); }

  template <typename Models>
  auto& models() const
  {
    static_assert(
        std::is_same<Models, Process::ProcessModel>::value,
        "Interval component must be passed Process::ProcessModel as child.");

    SCORE_ASSERT(m_interval);
    return m_interval->processes;
  }

protected:
  QPointer<Scenario::IntervalModel> m_interval;
};

template <typename System_T>
using GenericIntervalComponent = Scenario::IntervalComponent<score::GenericComponent<System_T>>;
}
