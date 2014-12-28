#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"
#include "SettableIdentifier.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const SettableIdentifier& obj)
{
	m_stream << obj.m_id << obj.m_set;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SettableIdentifier& obj)
{
	m_stream >> obj.m_id >> obj.m_set;
}

template<>
void Visitor<Reader<JSON>>::readFrom(const SettableIdentifier& obj)
{
	m_obj["IdentifierSet"] = obj.m_set;
	m_obj["Identifier"] = obj.m_id;
}

template<>
void Visitor<Writer<JSON>>::writeTo(SettableIdentifier& obj)
{
	obj.m_set = m_obj["IdentifierSet"].toBool();
	obj.m_id = m_obj["Identifier"].toInt();
}