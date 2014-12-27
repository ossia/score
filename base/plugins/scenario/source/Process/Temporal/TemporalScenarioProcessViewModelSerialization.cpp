#include "TemporalScenarioProcessViewModelSerialization.hpp"
#include "Process/AbstractScenarioProcessViewModel.hpp"
#include "TemporalScenarioProcessViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::visit(const TemporalScenarioProcessViewModel& pvm)
{
	auto constraints = constraintsViewModels(pvm);

	m_stream << (int) constraints.size();
	for(const TemporalScenarioProcessViewModel::constraint_view_model_type* constraint : constraints)
	{
		// TODO
		//m_stream << constraint->model()->id();
		//m_stream << constraint;
	}
}

template<>
void Visitor<Writer<DataStream>>::visit(TemporalScenarioProcessViewModel&)
{
	int count;
	m_stream >> count;

	for(; count --> 0;)
	{
		int __warn;
		// TODO
		//pvm.makeConstraintViewModel(s);
	}
}

QDataStream& operator >>(QDataStream& s, TemporalScenarioProcessViewModel& pvm)
{
	/*
	int count;
	s >> count;

	for(; count --> 0;)
	{
		pvm.makeConstraintViewModel(s);
	}

	return s;*/
}
