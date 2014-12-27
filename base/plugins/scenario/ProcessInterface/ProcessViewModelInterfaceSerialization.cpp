#include "ProcessViewModelInterfaceSerialization.hpp"
#include "ProcessSharedModelInterface.hpp"
#include "ProcessViewModelInterface.hpp"
#include "Document/Constraint/ConstraintModel.hpp"


ProcessViewModelInterface* createProcessViewModel(Deserializer<DataStream>& deserializer,
												  ConstraintModel* constraint,
												  QObject* parent)
{
	SettableIdentifier sharedProcessId;
	deserializer.m_stream >> sharedProcessId;

	auto process = constraint->process(sharedProcessId);
	auto viewmodel = process->makeViewModel(DataStream::type(),
											static_cast<void*>(&deserializer),
											parent);

	return viewmodel;
}


template<>
void Visitor<Reader<DataStream>>::visit(const ProcessViewModelInterface& processViewModel)
{
	// To allow recration using createProcessViewModel.
	// This supposes that the process is stored inside a Constraint.
	m_stream << processViewModel.sharedProcessModel()->id();

	visit(static_cast<const IdentifiedObject&>(processViewModel));

	// ProcessViewModelInterface doesn't have any particular data to save

	// Save the subclass
	processViewModel.serialize(DataStream::type(),
							   static_cast<void*>(this));
}

