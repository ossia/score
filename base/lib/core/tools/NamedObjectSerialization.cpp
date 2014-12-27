#include "NamedObjectSerialization.hpp"

#include "NamedObject.hpp"

template<>
void Visitor<Reader<DataStream>>::visit<NamedObject>(NamedObject& obj)
{
	m_stream << obj.objectName();
}

template<>
void Visitor<Writer<DataStream>>::visit<NamedObject>(NamedObject& obj)
{
	QString objectName;
	m_stream >> objectName;
	obj.setObjectName(objectName);
}
