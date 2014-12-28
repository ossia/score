#include "interface/serialization/DataStreamVisitor.hpp"
#include "NamedObject.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const NamedObject& obj)
{
	m_stream << obj.objectName();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(NamedObject& obj)
{
	QString objectName;
	m_stream >> objectName;
	obj.setObjectName(objectName);
}
