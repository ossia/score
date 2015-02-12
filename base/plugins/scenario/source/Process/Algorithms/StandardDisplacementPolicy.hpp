#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <tools/SettableIdentifier.hpp>

class ScenarioProcessSharedModel;
class EventModel;
class ConstraintModel;
class TimeNodeModel;

namespace StandardDisplacementPolicy
{
	void setEventPosition(ScenarioProcessSharedModel& scenario,
						  id_type<EventModel> eventId,
						  TimeValue absolute_time,
						  double heightPosition);

	void setConstraintPosition(ScenarioProcessSharedModel& scenario,
							   id_type<ConstraintModel> constraintId,
							   TimeValue absolute_time,
							   double heightPosition);
}
