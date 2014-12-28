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
	for(auto process : processes)
	{
		readFrom(*process);
	}

	// Boxes
	auto boxes = constraint.boxes();
	m_stream	<<  (int) boxes.size();
	for(auto box : boxes)
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
	int process_count;
	m_stream >> process_count;
	for(; process_count --> 0;)
	{
		constraint.addProcess(createProcess(*this, &constraint));
	}

	// Boxes
	int box_count;
	m_stream >> box_count;
	for(; box_count --> 0;)
	{
		constraint.addBox(new BoxModel(*this, &constraint));
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





template<> void Visitor<Reader<JSON>>::readFrom(const ConstraintModel& constraint)
{
	readFrom(static_cast<const IdentifiedObject&>(constraint));
	m_obj["Metadata"] = QVariant::fromValue(constraint.metadata).toJsonObject();
	m_obj["HeightPercentage"] = constraint.heightPercentage();
	m_obj["StartEvent"] = constraint.startEvent();
	m_obj["EndEvent"] = constraint.endEvent();

	// Processes
	QJsonArray process_array;
	for(auto process : constraint.processes())
	{
		process_array.push_back(toJsonObject(*process));
	}
	m_obj["Processes"] = process_array;

	// Boxes
	QJsonArray box_array;
	for(auto box : constraint.boxes())
	{
		box_array.push_back(toJsonObject(*box));
	}
	m_obj["Boxes"] = box_array;

	// API Object
	// s << i.apiObject()->save();
	// Things that should be queried from the API :
	m_obj["Width"] = constraint.width();
	m_obj["StartDate"] = constraint.startDate();
}

template<> void Visitor<Writer<JSON>>::writeTo(ConstraintModel& constraint)
{
	constraint.metadata = QJsonValue(m_obj["Metadata"]).toVariant().value<ConstraintModelMetadata>();
	constraint.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
	constraint.setStartEvent(m_obj["StartEvent"].toInt());
	constraint.setEndEvent(m_obj["EndEvent"].toInt());

	QJsonArray process_array = m_obj["Processes"].toArray();
	for(auto json_vref : process_array)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		constraint.addProcess(createProcess(deserializer, &constraint));
	}

	QJsonArray box_array = m_obj["Boxes"].toArray();
	for(auto json_vref : box_array)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		constraint.addBox(new BoxModel(deserializer, &constraint));
	}

	// Things that should be queried from the API :
	constraint.setWidth(m_obj["Width"].toInt());
	constraint.setStartDate(m_obj["StartDate"].toInt());
}