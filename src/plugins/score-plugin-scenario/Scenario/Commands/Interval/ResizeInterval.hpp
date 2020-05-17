#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score_plugin_scenario_export.h>

/** README
 *
 * So, I know what you are thinking right now:
 * "okay, it's the command to resize intervals, it must not be too hard,
 * just add / remove the date to the current interval date and live happily
 * forever after"
 *
 * Followed by the reading of the following file, and the inevitable:
 *
 * "WTF why do they need friggin interfaces and abstract factories to resize a
 * goddamn interval what kind of madness is this i don't even
 *
 * ...
 *
 * ...
 *
 * o sh*t the implementation for Scenario calls to MoveBaseEvent which calls to
 * another abstract factory pls send help who the hell designed this even the
 * windows API is better
 * "
 *
 * Here are the problems with resizing intervals :
 * 1/ it may move other parts of the scenario depending on the flexible-ness
 * policy, which is a **complicated** research problem involving constraint
 * satisfiaction, implemented in an external CSP addon because it's so damn
 * unstable. This is why there is an abstraction of the behaviour in
 * MoveBaseEvent: multiple displacement algorithms are possible.
 *
 * 2/ it may behave differently in different cases :
 *  - if the interval is in a scenario
 *  - or in a loop
 *  - or if it's the base interval of the score
 *
 * these three cases have different requirements - and multiple additional
 * processes leveraging intervals are being developed, each with their specific
 * resizing rules. Hence, it **has** to be abstract in some way.
 *
 */

namespace score
{
class Command;
}
namespace Scenario
{
class IntervalModel;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalResizer : public score::InterfaceBase
{
  SCORE_INTERFACE(IntervalResizer, "8db5b613-a9a8-4a49-9e89-6c07e7117518")
public:
  virtual bool matches(const IntervalModel& m) const noexcept = 0;

  virtual score::Command* make(
      const IntervalModel& itv,
      TimeVal new_duration = TimeVal{1000},
      ExpandMode = ExpandMode::GrowShrink,
      LockMode = LockMode::Free) const noexcept = 0;

  virtual void update(
      score::Command& cmd,
      const IntervalModel& interval,
      TimeVal new_duration,
      ExpandMode = ExpandMode::GrowShrink,
      LockMode = LockMode::Free) const noexcept = 0;
};

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalResizerList final
    : public score::InterfaceList<IntervalResizer>
{
public:
  ~IntervalResizerList() override;

  IntervalResizer* find(const IntervalModel& m) const
  {
    using val_t = decltype(*this->begin());
    auto it = ossia::find_if(*this, [&](val_t& elt) { return elt.matches(m); });
    if (it != this->end())
      return &(*it);
    return nullptr;
  }

  template <typename... Args>
  score::Command* make(const IntervalModel& m, Args&&... args) const noexcept
  {
    using val_t = decltype(*this->begin());
    auto it = ossia::find_if(*this, [&](val_t& elt) { return elt.matches(m); });

    return (it != this->end()) ? (*it).make(m, std::forward<Args>(args)...)
                               : (score::Command*)nullptr;
  }
};

class ScenarioIntervalResizer final : public IntervalResizer
{
  SCORE_CONCRETE("1a91f756-da39-4d20-947a-ea08a80e7b8e")

  bool matches(const IntervalModel& m) const noexcept override;
  score::Command* make(const IntervalModel& itv, TimeVal new_duration, ExpandMode, LockMode)
      const noexcept override;
  void update(
      score::Command& cmd,
      const IntervalModel& interval,
      TimeVal new_duration,
      ExpandMode,
      LockMode) const noexcept override;
};
class BaseScenarioIntervalResizer final : public IntervalResizer
{
  SCORE_CONCRETE("4b2ba7d3-2f93-43e0-a034-94c88b74f110")

  bool matches(const IntervalModel& m) const noexcept override;
  score::Command* make(const IntervalModel& itv, TimeVal new_duration, ExpandMode, LockMode)
      const noexcept override;
  void update(
      score::Command& cmd,
      const IntervalModel& interval,
      TimeVal new_duration,
      ExpandMode,
      LockMode) const noexcept override;
};

}
