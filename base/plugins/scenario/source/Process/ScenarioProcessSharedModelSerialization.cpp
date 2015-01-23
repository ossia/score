#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"
#include "ScenarioProcessSharedModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioProcessSharedModel& scenario)
{
	m_stream << scenario.m_startEventId;
	m_stream << scenario.m_endEventId;

	// Constraints
	auto constraints = scenario.constraints();
	m_stream << (int) constraints.size();
	for(auto constraint : constraints)
	{
		readFrom(*constraint);
	}

	// Events
	auto events = scenario.events();
	m_stream << (int) events.size();
	for(auto event : events)
	{
		readFrom(*event);
	}

	insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioProcessSharedModel& scenario)
{
	m_stream >> scenario.m_startEventId;
	m_stream >> scenario.m_endEventId;

	// Constraints
	int constraint_count;
	m_stream >> constraint_count;
	for(; constraint_count --> 0;)
	{
		auto constraint = new ConstraintModel{*this, &scenario};
		scenario.addConstraint(constraint);
	}

	// Events
	int event_count;
	m_stream >> event_count;
	for(; event_count --> 0;)
	{
		auto evmodel = new EventModel{*this, &scenario};
		scenario.addEvent(evmodel);
	}

	// Recreate the API
	/*for(ConstraintModel* constraint : scenario.m_constraints)
	{
		auto sev = scenario.event(constraint->startEvent());
		auto eev = scenario.event(constraint->endEvent());

		scenario.m_scenario->addTimeBox(*constraint->apiObject(),
										*sev->apiObject(),
										*eev->apiObject());
	}*/

	checkDelimiter();
}




template<>
void Visitor<Reader<JSON>>::readFrom(const ScenarioProcessSharedModel& scenario)
{
	m_obj["StartEventId"] = toJsonObject(scenario.m_startEventId);
	m_obj["EndEventId"] = toJsonObject(scenario.m_endEventId);

	QJsonArray constraints_array;
	for(auto constraint : scenario.constraints())
	{
		constraints_array.push_back(toJsonObject(*constraint));
	}
	m_obj["Constraints"] = constraints_array;

	QJsonArray events_array;
	for(auto event : scenario.events())
	{
		events_array.push_back(toJsonObject(*event));
	}
	m_obj["Events"] = events_array;
}

template<>
void Visitor<Writer<JSON>>::writeTo(ScenarioProcessSharedModel& scenario)
{
	scenario.m_startEventId = fromJsonObject<id_type<EventModel>>(m_obj["StartEventId"].toObject());
	scenario.m_endEventId = fromJsonObject<id_type<EventModel>>(m_obj["EndEventId"].toObject());
	QJsonArray constraints_array = m_obj["Constraints"].toArray();
	for(auto json_vref : constraints_array)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		auto constraint = new ConstraintModel{deserializer,
											  &scenario};
		scenario.addConstraint(constraint);
	}

	QJsonArray events_array = m_obj["Events"].toArray();
	for(auto json_vref : events_array)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		auto evmodel = new EventModel{deserializer,
									  &scenario};
		scenario.addEvent(evmodel);
	}

	// Recreate the API
	/*for(ConstraintModel* constraint : scenario.m_constraints)
	{
		auto sev = scenario.event(constraint->startEvent());
		auto eev = scenario.event(constraint->endEvent());

		scenario.m_scenario->addTimeBox(*constraint->apiObject(),
										*sev->apiObject(),
										*eev->apiObject());
	}*/
}



void ScenarioProcessSharedModel::serialize(SerializationIdentifier identifier,
										   void* data) const
{
	if(identifier == DataStream::type())
	{
		static_cast<Serializer<DataStream>*>(data)->readFrom(*this);
		return;
	}
	else if(identifier == JSON::type())
	{
		static_cast<Serializer<JSON>*>(data)->readFrom(*this);
		return;
	}

	throw std::runtime_error("ScenarioSharedProcessModel only supports DataStream & JSON serialization");
}

#include "ScenarioProcessFactory.hpp"
ProcessSharedModelInterface* ScenarioProcessFactory::makeModel(SerializationIdentifier identifier,
															  void* data,
															  QObject* parent)
{
	if(identifier == DataStream::type())
	{
		return new ScenarioProcessSharedModel{*static_cast<Deserializer<DataStream>*>(data), parent};
	}
	else if(identifier == JSON::type())
	{
		return new ScenarioProcessSharedModel{*static_cast<Deserializer<JSON>*>(data), parent};
	}

	throw std::runtime_error("ScenarioSharedProcessModel only supports DataStream & JSON serialization");
}

ProcessViewModelInterface* ScenarioProcessSharedModel::makeViewModel(SerializationIdentifier identifier,
																	 void* data,
																	 QObject* parent)
{
	if(identifier == DataStream::type())
	{
		auto scen = new TemporalScenarioProcessViewModel(*static_cast<Deserializer<DataStream>*>(data),
														 this,
														 parent);
		makeViewModel_impl(scen);
		return scen;
	}
	else if(identifier == JSON::type())
	{
		auto scen = new TemporalScenarioProcessViewModel(*static_cast<Deserializer<JSON>*>(data),
														 this,
														 parent);
		makeViewModel_impl(scen);
		return scen;
	}

	throw std::runtime_error("ScenarioProcessViewModels only supports DataStream & JSON serialization");
}
