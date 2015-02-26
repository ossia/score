#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "ScenarioModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioModel& scenario)
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

    auto timenodes = scenario.timeNodes();
    m_stream << (int) timenodes.size();

    for(auto timenode : timenodes)
    {
        readFrom(*timenode);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioModel& scenario)
{
    m_stream >> scenario.m_startEventId;
    m_stream >> scenario.m_endEventId;

    // Constraints
    int constraint_count;
    m_stream >> constraint_count;

    for(; constraint_count -- > 0;)
    {
        auto constraint = new ConstraintModel {*this, &scenario};
        scenario.addConstraint(constraint);
    }

    // Events
    int event_count;
    m_stream >> event_count;

    for(; event_count -- > 0;)
    {
        auto evmodel = new EventModel {*this, &scenario};
        scenario.addEvent(evmodel);
    }

    int timenode_count;
    m_stream >> timenode_count;

    for(; timenode_count -- > 0;)
    {
        auto tnmodel = new TimeNodeModel {*this, &scenario};
        scenario.m_timeNodes.push_back(tnmodel);
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
void Visitor<Reader<JSON>>::readFrom(const ScenarioModel& scenario)
{
    m_obj["StartEventId"] = toJsonObject(scenario.m_startEventId);
    m_obj["EndEventId"] = toJsonObject(scenario.m_endEventId);

    m_obj["Constraints"] = toJsonArray(scenario.constraints());
    m_obj["Events"] = toJsonArray(scenario.events());
    m_obj["TimeNodes"] = toJsonArray(scenario.timeNodes());
}

template<>
void Visitor<Writer<JSON>>::writeTo(ScenarioModel& scenario)
{
    scenario.m_startEventId = fromJsonObject<id_type<EventModel>> (m_obj["StartEventId"].toObject());
    scenario.m_endEventId = fromJsonObject<id_type<EventModel>> (m_obj["EndEventId"].toObject());

    for(auto json_vref : m_obj["Constraints"].toArray())
    {
        auto constraint = new ConstraintModel {Deserializer<JSON>{json_vref.toObject() },
                                               &scenario
                                              };
        scenario.addConstraint(constraint);
    }

    for(auto json_vref : m_obj["Events"].toArray())
    {
        auto evmodel = new EventModel {Deserializer<JSON>{json_vref.toObject() },
                                       &scenario
                                      };
        scenario.addEvent(evmodel);
    }

    for(auto json_vref : m_obj["TimeNodes"].toArray())
    {
        auto tnmodel = new TimeNodeModel {Deserializer<JSON>{json_vref.toObject() },
                                          &scenario
                                         };

        scenario.m_timeNodes.push_back(tnmodel);
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



void ScenarioModel::serialize(SerializationIdentifier identifier,
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

    throw std::runtime_error("ScenarioProcessModel only supports DataStream & JSON serialization");
}

#include "ScenarioFactory.hpp"
ProcessSharedModelInterface* ScenarioFactory::makeModel(SerializationIdentifier identifier,
        void* data,
        QObject* parent)
{
    if(identifier == DataStream::type())
    {
        return new ScenarioModel {*static_cast<Deserializer<DataStream>*>(data), parent};
    }
    else if(identifier == JSON::type())
    {
        return new ScenarioModel {*static_cast<Deserializer<JSON>*>(data), parent};
    }

    throw std::runtime_error("ScenarioProcessModel only supports DataStream & JSON serialization");
}

ProcessViewModelInterface* ScenarioModel::makeViewModel(SerializationIdentifier identifier,
        void* data,
        QObject* parent)
{
    if(identifier == DataStream::type())
    {
        auto scen = new TemporalScenarioViewModel(*static_cast<Deserializer<DataStream>*>(data),
                this,
                parent);
        makeViewModel_impl(scen);
        return scen;
    }
    else if(identifier == JSON::type())
    {
        auto scen = new TemporalScenarioViewModel(*static_cast<Deserializer<JSON>*>(data),
                this,
                parent);
        makeViewModel_impl(scen);
        return scen;
    }

    throw std::runtime_error("ScenarioViewModels only supports DataStream & JSON serialization");
}
