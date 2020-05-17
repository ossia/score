#pragma once
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <QPointer>

#include <verdigris>
namespace Scenario
{
class EventModel;
class StateModel;
class IntervalModel;
class TimeSyncModel;
}

Q_DECLARE_METATYPE(Id<Scenario::TimeSyncModel>)
W_REGISTER_ARGTYPE(Id<Scenario::TimeSyncModel>)

Q_DECLARE_METATYPE(Id<Scenario::EventModel>)
W_REGISTER_ARGTYPE(Id<Scenario::EventModel>)
W_REGISTER_ARGTYPE(Scenario::EventModel)
W_REGISTER_ARGTYPE(const Scenario::EventModel&)
W_REGISTER_ARGTYPE(Scenario::EventModel&)

Q_DECLARE_METATYPE(Id<Scenario::StateModel>)
W_REGISTER_ARGTYPE(Id<Scenario::StateModel>)
W_REGISTER_ARGTYPE(Scenario::StateModel)
W_REGISTER_ARGTYPE(const Scenario::StateModel&)
W_REGISTER_ARGTYPE(Scenario::StateModel&)

Q_DECLARE_METATYPE(Id<Scenario::IntervalModel>)
Q_DECLARE_METATYPE(Path<Scenario::IntervalModel>)

W_REGISTER_ARGTYPE(Id<Scenario::IntervalModel>)
W_REGISTER_ARGTYPE(OptionalId<Scenario::IntervalModel>)
W_REGISTER_ARGTYPE(Path<Scenario::IntervalModel>)
W_REGISTER_ARGTYPE(Scenario::IntervalModel)
W_REGISTER_ARGTYPE(Scenario::IntervalModel&)
