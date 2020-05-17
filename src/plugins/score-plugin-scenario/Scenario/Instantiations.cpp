#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__)

#include <Scenario/Instantiations.hpp>
#include <Scenario/PresenterInstantiations.hpp>

template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::IntervalModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::EventModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::TimeSyncModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::StateModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::CommentBlockModel>;

template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::StatePresenter, Scenario::StateModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::EventPresenter, Scenario::EventModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::GraphalIntervalPresenter, Scenario::IntervalModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::CommentBlockPresenter, Scenario::CommentBlockModel, void>;

#endif
