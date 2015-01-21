#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"
#include "IdentifiedObject.hpp"

/*
template<>
void Visitor<Reader<DataStream>>::readFrom(const IdentifiedObject& obj)
{
	readFrom(static_cast<const NamedObject&>(obj));
	readFrom(obj.id());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(IdentifiedObject& obj)
{
	SettableIdentifier id;
	writeTo(id);
	obj.setId(std::move(id));
}

template<>
void Visitor<Reader<JSON>>::readFrom(const IdentifiedObject& obj)
{
	readFrom(static_cast<const NamedObject&>(obj));
	m_obj["IdentifierSet"] = obj.id().set();
	m_obj["Identifier"] = (int) obj.id();
}

template<>
void Visitor<Writer<JSON>>::writeTo(IdentifiedObject& obj)
{
	if(m_obj["IdentifierSet"].toBool())
	{
		obj.setId(SettableIdentifier((int)m_obj["Identifier"].toInt()));
	}
}*/
