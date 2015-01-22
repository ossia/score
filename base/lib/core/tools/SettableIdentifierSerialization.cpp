
#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"
#include "SettableIdentifier.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const boost::optional<int32_t>& obj)
{
	m_stream << obj.is_initialized();

	if(obj.is_initialized())
		m_stream << obj.value();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(boost::optional<int32_t>& obj)
{
	bool b{};
	m_stream >> b;

	if(b)
	{
		int32_t val;
		m_stream >> val;

		obj = val;
	}
	else
	{
		obj.reset();
	}
}

template<>
void Visitor<Reader<JSON>>::readFrom(const boost::optional<int32_t>& obj)
{
	m_obj["IdentifierSet"] = obj.is_initialized();

	if(obj.is_initialized())
		m_obj["Identifier"] = obj.value();
}

template<>
void Visitor<Writer<JSON>>::writeTo(boost::optional<int32_t>& obj)
{
	if(m_obj["IdentifierSet"].toBool())
	{
		obj = m_obj["Identifier"].toInt();
	}
	else
	{
		obj.reset();
	}
}
