#include "ConstraintSerializer.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

template<> void Visitor<JSONReader>::visit<ConstraintModel>(ConstraintModel&)
{

}


template<> void Visitor<DataStreamReader>::visit<ConstraintModel>(ConstraintModel& c)
{
	m_stream << c;
}

template<> void Visitor<DataStreamWriter>::visit<ConstraintModel>(ConstraintModel& c)
{
	m_stream >> c;
}



QDataStream& operator <<(QDataStream& s, const ConstraintModel& constraint)
{
	s << static_cast<const IdentifiedObject&>(constraint);

	// Metadata
	s	<< constraint.metadata
		<< constraint.heightPercentage();

	// Processes
	auto processes = constraint.processes();
	s	<< (int) processes.size();
	for(auto& process : processes)
	{
		ConstraintModel::saveProcess(s, process);
	}

	// Boxes
	auto boxes = constraint.boxes();
	s	<<  (int) boxes.size();
	for(auto& box : boxes)
	{
		s << *box;
	}

	// Events
	s	<< constraint.startEvent();
	s	<< constraint.endEvent();

	// API Object
	// s << i.apiObject()->save();
	// Things that should be queried from the API :
	s << constraint.width()
	  << constraint.startDate();

	return s;
}


QDataStream& operator >>(QDataStream& s, ConstraintModel& constraint)
{
	double heightPercentage;
	s >> constraint.metadata >> heightPercentage;

	constraint.setHeightPercentage(heightPercentage);

	// Processes
	int process_size;
	s >> process_size;
	for(int i = 0; i < process_size; i++)
	{
		constraint.createProcess(s);
	}

	// Boxes
	int content_models_size;
	s >> content_models_size;
	for(int i = 0; i < content_models_size; i++)
	{
		constraint.createBox(s);
	}

	// Events
	int startId{}, endId{};
	s >> startId;
	s >> endId;
	constraint.setStartEvent(startId);
	constraint.setEndEvent(endId);

	// Things that should be queried from the API :
	int width{}, startDate{};
	s >> width
	  >> startDate;
	constraint.setWidth(width);
	constraint.setStartDate(startDate);

	return s;
}

void test()
{
	ConstraintModel m{0, nullptr};
	Visitor<JSONReader> v;
	v.visit(m);

	QByteArray arr;
	Visitor<DataStreamReader> v2(&arr);
	v2.visit(m);
}