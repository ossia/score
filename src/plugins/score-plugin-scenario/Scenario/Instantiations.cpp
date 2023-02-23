#include <score/config.hpp>

#if SCORE_EXTERN_TEMPLATES_IN_SHARED_LIBRARIES

#include <Scenario/Instantiations.hpp>
#include <Scenario/PresenterInstantiations.hpp>

template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::IntervalModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::EventModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::TimeSyncModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::StateModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    score::EntityMap<Scenario::CommentBlockModel>;

template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::StatePresenter, Scenario::StateModel, false>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::EventPresenter, Scenario::EventModel, false>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel, false>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel, false>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::GraphalIntervalPresenter, Scenario::IntervalModel, false>;
template class SCORE_PLUGIN_SCENARIO_EXPORT
    IdContainer<Scenario::CommentBlockPresenter, Scenario::CommentBlockModel, false>;

#endif
