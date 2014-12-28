#include "ProcessSharedModelInterfaceSerialization.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"

#include "Control/ProcessList.hpp"
#include "ProcessSharedModelInterface.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const ProcessSharedModelInterface& process)
{
	// To allow recration using createProcess
	m_stream << process.processName();

	readFrom(static_cast<const IdentifiedObject&>(process));

	// ProcessSharedModelInterface doesn't have any particular data to save

	// Save the subclass
	process.serialize(DataStream::type(),
					  static_cast<void*>(this));
}

template<>
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
void Visitor<Reader<JSON>>::readFrom(const ProcessSharedModelInterface& process)
{
	// To allow recration using createProcess
	m_obj["ProcessName"] = process.processName();

	readFrom(static_cast<const IdentifiedObject&>(process));

	// ProcessSharedModelInterface doesn't have any particular data to save

	// Save the subclass
	process.serialize(JSON::type(),
					  static_cast<void*>(this));
}

template<>
ProcessSharedModelInterface* createProcess(Deserializer<JSON>& deserializer,
										   QObject* parent)
{
	auto model = ProcessList::getFactory(deserializer.m_obj["ProcessName"].toString())
					->makeModel(JSON::type(),
								static_cast<void*>(&deserializer),
								parent);

	return model;
}

