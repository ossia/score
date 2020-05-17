#pragma once
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__)
extern template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::IntervalModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::EventModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::TimeSyncModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::StateModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::CommentBlockModel>;
#endif
