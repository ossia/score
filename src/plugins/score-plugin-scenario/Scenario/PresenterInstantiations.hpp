#pragma once
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/Graph/GraphIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>

#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__)
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::StatePresenter, Scenario::StateModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::EventPresenter, Scenario::EventModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::GraphalIntervalPresenter, Scenario::IntervalModel>;
extern template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::CommentBlockPresenter, Scenario::CommentBlockModel>;
#endif
