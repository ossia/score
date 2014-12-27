#include "IdentifiedObjectSerialization.hpp"

#include "NamedObjectSerialization.hpp"
#include "IdentifiedObject.hpp"


template<>
void Visitor<Reader<DataStream>>::visit(const IdentifiedObject& obj)
{
	visit(static_cast<const NamedObject&>(obj));

	m_stream << obj.id();
}

template<>
void Visitor<Writer<DataStream>>::visit<IdentifiedObject>(IdentifiedObject& obj)
{
	visit(static_cast<NamedObject&>(obj));

	SettableIdentifier id;
	m_stream >> id;
	obj.setId(std::move(id));
}
