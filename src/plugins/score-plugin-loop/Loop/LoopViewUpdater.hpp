#pragma once
#include <Scenario/Document/VerticalExtent.hpp>

namespace Scenario
{
class EventPresenter;
class TimeSyncPresenter;
class TemporalIntervalPresenter;
class StatePresenter;
}
namespace Loop
{
class LayerPresenter;
class ViewUpdater
{
public:
  ViewUpdater(LayerPresenter& presenter);

  void updateEvent(const Scenario::EventPresenter& event);

  void updateInterval(const Scenario::TemporalIntervalPresenter& pres);

  void updateTimeSync(const Scenario::TimeSyncPresenter& timesync);

  void updateState(const Scenario::StatePresenter& state);

  LayerPresenter& m_presenter;

  static const Scenario::VerticalExtent extent() { return {20., 50.}; }
};
}
