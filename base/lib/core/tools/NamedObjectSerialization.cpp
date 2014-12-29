#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"
#include "NamedObject.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const NamedObject& namedObject)
{
	m_stream << namedObject.objectName();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(NamedObject& namedObject)
{
	QString objectName;
	m_stream >> objectName;
	namedObject.setObjectName(objectName);
}


template<>
void Visitor<Reader<JSON>>::readFrom(const NamedObject& namedObject)
{
	m_obj["ObjectName"] = namedObject.objectName();
}

template<>
void Visitor<Writer<JSON>>::writeTo(NamedObject& namedObject)
{
	namedObject.setObjectName(m_obj["ObjectName"].toString());

}