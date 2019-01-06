#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class IntervalModel;
class IntervalPresenter;
class TemporalIntervalPresenter;
class IntervalView;
class TemporalIntervalView;
class EventModel;
class EventPresenter;
class EventView;
class StateModel;
class StatePresenter;
class StateView;
class TimeSyncModel;
class TimeSyncPresenter;
class TimeSyncView;
}
namespace Sequence
{
class ProcessModel;

using  IntervalModel             = Scenario::IntervalModel;
using  IntervalPresenter = Scenario::IntervalPresenter;
using  TemporalIntervalPresenter = Scenario::TemporalIntervalPresenter;
using  IntervalView      = Scenario::IntervalView;
using  TemporalIntervalView      = Scenario::TemporalIntervalView;
using  EventModel                = Scenario::EventModel;
using  EventPresenter            = Scenario::EventPresenter;
using  EventView                 = Scenario::EventView;
using  StateModel                = Scenario::StateModel;
using  StatePresenter            = Scenario::StatePresenter;
using  StateView                 = Scenario::StateView;
using  TimeSyncModel             = Scenario::TimeSyncModel;
using  TimeSyncPresenter         = Scenario::TimeSyncPresenter;
using  TimeSyncView              = Scenario::TimeSyncView;
}

PROCESS_METADATA(
    SCORE_PLUGIN_SCENARIO_EXPORT, Sequence::ProcessModel,
    "43420fd3-f12a-4866-9690-5795a4f28be9", "Sequence", "Sequence",
    Process::ProcessCategory::Structure, "Structure", "Temporal structure",
    "ossia score", {}, {}, {},
    Process::ProcessFlags::SupportsTemporal
        | Process::ProcessFlags::PutInNewSlot)
