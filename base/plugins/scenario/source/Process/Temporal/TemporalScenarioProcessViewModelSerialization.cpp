#include "TemporalScenarioProcessViewModelSerialization.hpp"
#include "Process/AbstractScenarioProcessViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModelSerialization.hpp"
#include "TemporalScenarioProcessViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalScenarioProcessViewModel& pvm)
{
	auto constraints = constraintsViewModels(pvm);

	m_stream << (int) constraints.size();
	for(auto constraint : constraints)
	{
		readFrom(*constraint);
	}
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TemporalScenarioProcessViewModel& pvm)
{
	int count;
	m_stream >> count;

	for(; count --> 0;)
	{
		auto cstr = createConstraintViewModel(*this, &pvm);
		pvm.addConstraintViewModel(cstr);
	}
}



template<>
void Visitor<Reader<JSON>>::readFrom(const TemporalScenarioProcessViewModel& pvm)
{
	QJsonArray arr;
	for(auto cstrvm : constraintsViewModels(pvm))
	{
		arr.push_back(toJsonObject(*cstrvm));
	}

	m_obj["Constraints"] = arr;
}

template<>
void Visitor<Writer<JSON>>::writeTo(TemporalScenarioProcessViewModel& pvm)
{
	QJsonArray arr = m_obj["Constraints"].toArray();

	for(auto json_vref : arr)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		auto cstrvm = createConstraintViewModel(deserializer,
												&pvm);
		pvm.addConstraintViewModel(cstrvm);
	}
}



void TemporalScenarioProcessViewModel::serialize(SerializationIdentifier identifier, void* data) const
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

	throw std::runtime_error("ScenarioProcessViewModel only supports DataStream serialization");
}