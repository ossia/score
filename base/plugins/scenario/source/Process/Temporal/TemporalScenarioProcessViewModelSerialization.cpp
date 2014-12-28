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
