#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>

extern template class score::EntityMap<Scenario::IntervalModel>;
extern template class score::EntityMap<Scenario::EventModel>;
extern template class score::EntityMap<Scenario::TimeSyncModel>;
extern template class score::EntityMap<Scenario::StateModel>;
extern template class score::EntityMap<Scenario::CommentBlockModel>;

extern template class IdContainer<
    Scenario::StatePresenter,
    Scenario::StateModel>;
extern template class IdContainer<
    Scenario::EventPresenter,
    Scenario::EventModel>;
extern template class IdContainer<
    Scenario::TimeSyncPresenter,
    Scenario::TimeSyncModel>;
extern template class IdContainer<
    Scenario::TemporalIntervalPresenter,
    Scenario::IntervalModel>;
extern template class IdContainer<
    Scenario::CommentBlockPresenter,
    Scenario::CommentBlockModel>;
