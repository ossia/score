#include "ConstraintModelSerialization.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterfaceSerialization.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"



QDataStream& operator<<(QDataStream& s, const ConstraintModelMetadata& m)
{
	s << m.name() << m.comment() << m.color();

	return s;
}

QDataStream& operator>>(QDataStream& s, ConstraintModelMetadata& m)
{
	QString name, comment;
	QColor color;
	s >> name >> comment >> color;

	m.setName(name);
	m.setComment(comment);
	m.setColor(color);

	return s;
}


// Note : comment gérer le cas d'un process shared model qui ne sait se sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?

template<> void Visitor<Reader<DataStream>>::readFrom(const ConstraintModel& constraint)
{
	readFrom(static_cast<const IdentifiedObject&>(constraint));

	// Metadata
	m_stream	<< constraint.metadata
				<< constraint.heightPercentage();

	// Processes
	auto processes = constraint.processes();
	m_stream	<< (int) processes.size();
	for(const ProcessSharedModelInterface* process : processes)
	{
		readFrom(*process);
	}

	// Boxes
	auto boxes = constraint.boxes();
	m_stream	<<  (int) boxes.size();
	for(const BoxModel* box : boxes)
	{
		readFrom(*box);
	}

	// Events
	m_stream	<< constraint.startEvent();
	m_stream	<< constraint.endEvent();

	// API Object
	// s << i.apiObject()->save();
	// Things that should be queried from the API :
	m_stream << constraint.width()
			 << constraint.startDate();
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintModel& constraint)
{
	double heightPercentage;
	m_stream >> constraint.metadata >> heightPercentage;

	constraint.setHeightPercentage(heightPercentage);

	// Processes
	int process_size;
	m_stream >> process_size;
	for(int i = 0; i < process_size; i++)
	{
		constraint.addProcess(createProcess(*this, &constraint));
	}

	// Boxes
	int content_models_size;
	m_stream >> content_models_size;
	for(int i = 0; i < content_models_size; i++)
	{
		BoxModel* box = new BoxModel(*this, &constraint);
		constraint.addBox(box);
	}

	// Events
	int startId{}, endId{};
	m_stream >> startId;
	m_stream >> endId;
	constraint.setStartEvent(startId);
	constraint.setEndEvent(endId);

	// Things that should be queried from the API :
	int width{}, startDate{};
	m_stream >> width
			 >> startDate;
	constraint.setWidth(width);
	constraint.setStartDate(startDate);
}