#include "ProcessSharedModelInterfaceSerialization.hpp"
#include "Control/ProcessList.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "core/tools/IdentifiedObjectSerialization.hpp"

ProcessSharedModelInterface* createProcess(Deserializer<DataStream>& deserializer,
										   QObject* parent)
{
	QString processName;
	deserializer.m_stream >> processName;

	auto model = ProcessList::getFactory(processName)
					->makeModel(DataStream::type(),
								static_cast<void*>(&deserializer),
								parent);

	return model;
}


template<>
void Visitor<Reader<DataStream>>::visit<ProcessSharedModelInterface>(ProcessSharedModelInterface& process)
{
	// To allow recration using createProcess
	m_stream << process.processName();

	visit(static_cast<IdentifiedObject&>(process));

	// ProcessSharedModelInterface doesn't have any particular data to save

	// Save the subclass
	process.serialize(DataStream::type(),
					  static_cast<void*>(this));
}

