#include "interface/serialization/DataStreamVisitor.hpp"
#include "NamedObjectSerialization.hpp"
#include "IdentifiedObject.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const IdentifiedObject& obj)
{
	readFrom(static_cast<const NamedObject&>(obj));

	m_stream << obj.id();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(IdentifiedObject& obj)
{
	SettableIdentifier id;
	m_stream >> id;
	obj.setId(std::move(id));
}
